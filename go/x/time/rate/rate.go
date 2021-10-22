// Copyright 2015 The Go Authors. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

// Package rate provides a rate limiter.
package rate

import (
	"context"
	"fmt"
	"math"
	"sync"
	"time"
)

// Limit defines the maximum frequency of some events.
// Limit is represented as number of events per second.
// A zero Limit allows no events.
type Limit float64

// Inf is the infinite rate limit; it allows all events (even if burst is zero).
const Inf = Limit(math.MaxFloat64)

// Every converts a minimum time interval between events to a Limit.
// 可以将时间转化为速率
// 例如：每5秒一个，转化为速率就是0.2一秒
func Every(interval time.Duration) Limit {
	if interval <= 0 {
		return Inf
	}
	return 1 / Limit(interval.Seconds())
}

// A Limiter controls how frequently events are allowed to happen.
// It implements a "token bucket" of size b, initially full and refilled
// at rate r tokens per second.
// Informally, in any large enough time interval, the Limiter limits the
// rate to r tokens per second, with a maximum burst size of b events.
// As a special case, if r == Inf (the infinite rate), b is ignored.
// See https://en.wikipedia.org/wiki/Token_bucket for more about token buckets.
//
// The zero value is a valid Limiter, but it will reject all events.
// Use NewLimiter to create non-zero Limiters.
//
// Limiter has three main methods, Allow, Reserve, and Wait.
// Most callers should use Wait.
//
// Each of the three methods consumes a single token.
// They differ in their behavior when no token is available.
// If no token is available, Allow returns false.
// If no token is available, Reserve returns a reservation for a future token
// and the amount of time the caller must wait before using it.
// If no token is available, Wait blocks until one can be obtained
// or its associated context.Context is canceled.
//
// The methods AllowN, ReserveN, and consume n tokens.
type Limiter struct {
	limit Limit // 向令牌桶中放置 Token 的速率(如：1 秒放置 10 个 Token)
	burst int // 令牌桶的容联

	mu     sync.Mutex
	tokens float64 // 令牌桶中目前剩余的token数目，可以为负数。
	// last is the last time the limiter's tokens field was updated
	last time.Time
	// lastEvent is the latest time of a rate-limited event (past or future)
	lastEvent time.Time
}

// Limit returns the maximum overall event rate.
func (lim *Limiter) Limit() Limit {
	lim.mu.Lock()
	defer lim.mu.Unlock()
	return lim.limit
}

// Burst returns the maximum burst size. Burst is the maximum number of tokens
// that can be consumed in a single call to Allow, Reserve, or Wait, so higher
// Burst values allow more events to happen at once.
// A zero Burst allows no events, unless limit == Inf.
func (lim *Limiter) Burst() int {
	return lim.burst
}

// NewLimiter returns a new Limiter that allows events up to rate r and permits
// bursts of at most b tokens.
func NewLimiter(r Limit, b int) *Limiter {
	return &Limiter{
		limit: r,
		burst: b,
	}
}

// Allow is shorthand for AllowN(time.Now(), 1).
func (lim *Limiter) Allow() bool {
	return lim.AllowN(time.Now(), 1)
}

// AllowN reports whether n events may happen at time now.
// Use this method if you intend to drop / skip events that exceed the rate limit.
// Otherwise use Reserve or Wait.
func (lim *Limiter) AllowN(now time.Time, n int) bool {
	return lim.reserveN(now, n, 0).ok
}

// A Reservation holds information about events that are permitted by a Limiter to happen after a delay.
// A Reservation may be canceled, which may enable the Limiter to permit additional events.
type Reservation struct {
	ok        bool
	lim       *Limiter
	tokens    int
	timeToAct time.Time
	// This is the Limit at reservation time, it can change later.
	limit Limit
}

// OK returns whether the limiter can provide the requested number of tokens
// within the maximum wait time.  If OK is false, Delay returns InfDuration, and
// Cancel does nothing.
func (r *Reservation) OK() bool {
	return r.ok
}

// Delay is shorthand for DelayFrom(time.Now()).
func (r *Reservation) Delay() time.Duration {
	return r.DelayFrom(time.Now())
}

// InfDuration is the duration returned by Delay when a Reservation is not OK.
const InfDuration = time.Duration(1<<63 - 1)

// DelayFrom returns the duration for which the reservation holder must wait
// before taking the reserved action.  Zero duration means act immediately.
// InfDuration means the limiter cannot grant the tokens requested in this
// Reservation within the maximum wait time.
func (r *Reservation) DelayFrom(now time.Time) time.Duration {
	if !r.ok {
		return InfDuration
	}
	delay := r.timeToAct.Sub(now)
	if delay < 0 {
		return 0
	}
	return delay
}

// Cancel is shorthand for CancelAt(time.Now()).
func (r *Reservation) Cancel() {
	r.CancelAt(time.Now())
	return
}

// CancelAt indicates that the reservation holder will not perform the reserved action
// and reverses the effects of this Reservation on the rate limit as much as possible,
// considering that other reservations may have already been made.
func (r *Reservation) CancelAt(now time.Time) {
	if !r.ok {
		return
	}

	r.lim.mu.Lock()
	defer r.lim.mu.Unlock()

	if r.lim.limit == Inf || r.tokens == 0 || r.timeToAct.Before(now) {
		return
	}

	// calculate tokens to restore
	// The duration between lim.lastEvent and r.timeToAct tells us how many tokens were reserved
	// after r was obtained. These tokens should not be restored.
	// 为什么新分配的就不算呢？
	// 因为可以cancel表示该Event尚未发生，如果已经发生，则在前面的if分支就return了;
	// 那么后面继续申请的Event.timeToAct必定大于当前的r.timeToAct，也是预支的;
	// 那么归还当前的token时，需要把已经预支的那部分除去，因为已经算是预消费了，不能再给后面申请的Event使用
	restoreTokens := float64(r.tokens) - r.limit.tokensFromDuration(r.lim.lastEvent.Sub(r.timeToAct))

	// 当小于0，表示已经都预支完了，不能归还了
	if restoreTokens <= 0 {
		return
	}
	// advance time to now
	now, _, tokens := r.lim.advance(now)
	// calculate new number of tokens
	tokens += restoreTokens
	if burst := float64(r.lim.burst); tokens > burst {
		tokens = burst
	}

	// update state
	r.lim.last = now // 这一点也很关键
	r.lim.tokens = tokens

	// 如果都相等，说明跟没消费一样。直接还原成上次的状态吧
	if r.timeToAct == r.lim.lastEvent {
		prevEvent := r.timeToAct.Add(r.limit.durationFromTokens(float64(-r.tokens)))
		if !prevEvent.Before(now) {
			r.lim.lastEvent = prevEvent
		}
	}

	return
}

// Reserve is shorthand for ReserveN(time.Now(), 1).
func (lim *Limiter) Reserve() *Reservation {
	return lim.ReserveN(time.Now(), 1)
}

// ReserveN returns a Reservation that indicates how long the caller must wait before n events happen.
// The Limiter takes this Reservation into account when allowing future events.
// ReserveN returns false if n exceeds the Limiter's burst size.
// Usage example:
//   r := lim.ReserveN(time.Now(), 1)
//   if !r.OK() {
//     // Not allowed to act! Did you remember to set lim.burst to be > 0 ?
//     return
//   }
//   time.Sleep(r.Delay())
//   Act()
// Use this method if you wish to wait and slow down in accordance with the rate limit without dropping events.
// If you need to respect a deadline or cancel the delay, use Wait instead.
// To drop or skip events exceeding rate limit, use Allow instead.
func (lim *Limiter) ReserveN(now time.Time, n int) *Reservation {
	r := lim.reserveN(now, n, InfDuration)
	return &r
}

// Wait 和 WaitN 方法都是用于消费令牌桶中的令牌，其中当 n=1 时， Wait 方法相当于是 WaitN(ctx，1)，n 表示一次从令牌桶中获取令牌的数量
// Wait is shorthand for WaitN(ctx, 1).
func (lim *Limiter) Wait(ctx context.Context) (err error) {
	return lim.WaitN(ctx, 1)
}

// WaitN blocks until lim permits n events to happen.
// It returns an error if n exceeds the Limiter's burst size, the Context is
// canceled, or the expected wait time exceeds the Context's Deadline.
// The burst limit is ignored if the rate limit is Inf.
func (lim *Limiter) WaitN(ctx context.Context, n int) (err error) {
	lim.mu.Lock()
	burst := lim.burst
	limit := lim.limit
	lim.mu.Unlock()
        // 在 limit 不等于 Inf 的前提下，如果说 n 大于令牌桶的容量则 WaitN 直接返回 error，不再执行下面的处理逻辑
	if n > burst && limit != Inf {
		return fmt.Errorf("rate: Wait(n=%d) exceeds limiter's burst %d", n, lim.burst)
	}
	// Check if ctx is already cancelled
	// 判断 context 是否被取消，如果 context 被取消了 WaitN 返回 error，不再执行下面的处理逻辑
	select {
	case <-ctx.Done():
		return ctx.Err()
	default:
	}
	
	// 下面是针对 n 没有大于令牌桶容量并且 context 也没有被取消的处理逻辑
	// Determine wait limit
	// now 获取当前时间
	now := time.Now()
	waitLimit := InfDuration
	// 判断 context 是否到了 deadline ，如果 context 到了 deadline 也就是 ok 为 true，则通过 Sub 方法计算出 context 被取消的时间
	// 和当前时间的差值->waitLimit
	if deadline, ok := ctx.Deadline(); ok {
		waitLimit = deadline.Sub(now)
	}

	// Reserve
	// 调用 reserveN 函数返回 Reservation 对象，相关细节查看 reserveN 函数的注释
	r := lim.reserveN(now, n, waitLimit)
        // reserveN 为 false 有两种情况：1、要获取的 token 数量大于令牌桶的容量 2、令牌桶所产生令牌的时间大于了需要等待的时间 也就是超出了 context deadline
	if !r.ok {
		return fmt.Errorf("rate: Wait(n=%d) would exceed context deadline", n)
	}
	// Wait if necessary
	// delay 为从当前时间开始，到满足条件，需要多长时间
	delay := r.DelayFrom(now)

	if delay == 0 {
		return nil
	}

	// 开一个timer，就等吧
	t := time.NewTimer(delay)
	defer t.Stop()
	select {
	case <-t.C:
		// We can proceed.
		return nil
	case <-ctx.Done():
		// Context was canceled before we could proceed.  Cancel the
		// reservation, which may permit other events to proceed sooner.
		r.Cancel()
		return ctx.Err()
	}
}

// SetLimit is shorthand for SetLimitAt(time.Now(), newLimit).
func (lim *Limiter) SetLimit(newLimit Limit) {
	lim.SetLimitAt(time.Now(), newLimit)
}

// SetLimitAt sets a new Limit for the limiter. The new Limit, and Burst, may be violated
// or underutilized by those which reserved (using Reserve or Wait) but did not yet act
// before SetLimitAt was called.
func (lim *Limiter) SetLimitAt(now time.Time, newLimit Limit) {
	lim.mu.Lock()
	defer lim.mu.Unlock()

	now, _, tokens := lim.advance(now)

	lim.last = now
	lim.tokens = tokens
	lim.limit = newLimit
}

// SetBurst is shorthand for SetBurstAt(time.Now(), newBurst).
func (lim *Limiter) SetBurst(newBurst int) {
	lim.SetBurstAt(time.Now(), newBurst)
}

// SetBurstAt sets a new burst size for the limiter.
func (lim *Limiter) SetBurstAt(now time.Time, newBurst int) {
	lim.mu.Lock()
	defer lim.mu.Unlock()

	now, _, tokens := lim.advance(now)

	lim.last = now
	lim.tokens = tokens
	lim.burst = newBurst
}

// reserveN is a helper method for AllowN, ReserveN, and WaitN.
// maxFutureReserve specifies the maximum reservation wait duration allowed.
// reserveN returns Reservation, not *Reservation, to avoid allocation in AllowN and WaitN.
// @parama now 当前系统时间
// @param n 要消费的token数量
// @param maxFutureReserve 愿意等待的最长时间
func (lim *Limiter) reserveN(now time.Time, n int, maxFutureReserve time.Duration) Reservation {
	lim.mu.Lock()

	// 如果没有限制
	if lim.limit == Inf {
		lim.mu.Unlock()
		return Reservation{
			ok:        true,
			lim:       lim,
			tokens:    n,
			timeToAct: now,
		}
	}
        
	// 以下为 limit 有限制的处理逻辑
	// 调用 advance 方法，获取当前桶中有多少令牌
	now, last, tokens := lim.advance(now)

	// Calculate the remaining number of tokens resulting from the request.
	// 计算取完 n 个令牌之后，桶还能剩能下多少token
	tokens -= float64(n)

	// Calculate the wait duration
	// 如果token < 0, 说明目前的token不够，需要等待一段时间，调用 durationFromTokens 函数计算生成所需要令牌数量需要多长时间
	var waitDuration time.Duration
	if tokens < 0 {
		waitDuration = lim.limit.durationFromTokens(-tokens)
	}

	// Decide result
	// 这里 ok 为 false 和 true 要分为几种情况：
	// true
	// 1、要获取的令牌数量不大于令牌桶的容量 并且 获取所需要的令牌等待时间不大于最大愿意等待时间(也就是我要的没有超出你的能力，并且我也愿意等待，只要你给我就行)
	// false
	// 1、我所需要的令牌数量你没有那么多，尽管我愿意等待
	// 2、我所需要的令牌数量没有超出的你能力，但是你现在没有那么多，你需要重新生产，可是你生产的时间太长，我等不了
	ok := n <= lim.burst && waitDuration <= maxFutureReserve

	// Prepare reservation
	// Reservation 对象
	r := Reservation{
		ok:    ok,
		lim:   lim,
		limit: lim.limit,
	}

	// timeToAct表示当桶中满足token数目等于n的时间
	if ok {
		r.tokens = n
		r.timeToAct = now.Add(waitDuration)
	}

	// Update state
	// 更新桶里面的token数目
	// 更新last时间
	// lastEvent
	if ok {
		lim.last = now
		lim.tokens = tokens
		lim.lastEvent = r.timeToAct
	} else {
		lim.last = last
	}

	lim.mu.Unlock()
	return r
}

// advance calculates and returns an updated state for lim resulting from the passage of time.
// lim is not changed.
// @param now 当前系统时间
// @return newNow 似乎还是这个now，没变
// @return newLast 如果 last > now, 则last为now
// @return newTokens 当前桶中应有的数目
func (lim *Limiter) advance(now time.Time) (newNow time.Time, newLast time.Time, newTokens float64) {
	// last is the last time the limiter's tokens field was updated
	// last 表示的是上一次从令牌桶中取出 Token 的时间
	last := lim.last
	// todo:这一处暂时没有看太懂
	if now.Before(last) {
		last = now
	}

	// Avoid making delta overflow below when last is very old.
	// durationFromTokens 函数主要用于计算生成 N 个 Token 需要多久，参数表示 Token 数量
	// 从下面的使用中可以看到，maxElapsed 表示将Token桶填满需要多久（令牌桶总容量 - 令牌桶中已有的令牌数量）
	// 为什么要拆分两步做，是为了防止后面的delta溢出
	
	// 为什么要拆分两步做，是为了防止后面的delta溢出，所谓的拆分成两步是指：1、先计算出填满令牌桶需要多久 2、当前时间到上一次取令牌这段时间内生成了多少 token 
	// 必须要保证当前时间到上一次取 token 的时间差不能大于填满令牌桶所需要的时间，因为默认情况下，last为0，此时delta算出来的，会非常大
	maxElapsed := lim.limit.durationFromTokens(float64(lim.burst) - lim.tokens)

	// elapsed 表示从当前到上次一共过去了多久
	// elapsed不能大于将桶填满的时间
	elapsed := now.Sub(last)
	if elapsed > maxElapsed {
		elapsed = maxElapsed
	}

	// Calculate the new number of tokens, due to time that passed.
	// 计算下过去这段时间，一共产生了多少token
	delta := lim.limit.tokensFromDuration(elapsed)

	// token取burst最大值，因为显然token数不能大于桶容量
	tokens := lim.tokens + delta
	if burst := float64(lim.burst); tokens > burst {
		tokens = burst
	}

	return now, last, tokens
}

// durationFromTokens is a unit conversion function from the number of tokens to the duration
// of time it takes to accumulate them at a rate of limit tokens per second.
// 将token转化为所需等待时间
func (limit Limit) durationFromTokens(tokens float64) time.Duration {
	seconds := tokens / float64(limit)
	return time.Nanosecond * time.Duration(1e9*seconds)
}

// tokensFromDuration is a unit conversion function from a time duration to the number of tokens
// which could be accumulated during that duration at a rate of limit tokens per second.
func (limit Limit) tokensFromDuration(d time.Duration) float64 {
	// Split the integer and fractional parts ourself to minimize rounding errors.
	// See golang.org/issues/34861.
	// 如果是用d.Seconds() * float64(limit), 因为d.Seconds是float64的。因此会造成精度的损失。
	// time.Duration是int64类型的，表示纳秒
	// time.Second
	sec := float64(d/time.Second) * float64(limit)
	nsec := float64(d%time.Second) * float64(limit)
	return sec + nsec/1e9
}
