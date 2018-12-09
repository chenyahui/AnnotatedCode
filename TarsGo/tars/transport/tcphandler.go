package transport

import (
	"context"
	"io"
	"net"
	"reflect"
	"strings"
	"sync/atomic"
	"time"

	"github.com/TarsCloud/TarsGo/tars/util/current"
	"github.com/TarsCloud/TarsGo/tars/util/gpool"
)

// Tcp网络处理器
type tcpHandler struct {
	conf *TarsServerConf // 一些配置

	lis *net.TCPListener
	ts  *TarsServer

	acceptNum   int32
	invokeNum   int32
	readBuffer  int
	writeBuffer int
	tcpNoDelay  bool // 是否开启NoDelay
	idleTime    time.Time
	gpool       *gpool.Pool
}

func (h *tcpHandler) Listen() (err error) {
	cfg := h.conf
	addr, err := net.ResolveTCPAddr("tcp4", cfg.Address)
	if err != nil {
		return err
	}
	// net返回的是个指针，因此用tcpHandler的Listener指向返回结果
	h.lis, err = net.ListenTCP("tcp4", addr)
	TLOG.Info("Listening on", cfg.Address)
	return
}

/*
  这个函数名有点歧义，实际上是一个不是handleConn处理一个连接，而是处理一个完整的包, pkg
*/
func (h *tcpHandler) handleConn(conn *net.TCPConn, pkg []byte) {
	handler := func() {
		ctx := context.Background()
		remoteAddr := conn.RemoteAddr().String()
		ipPort := strings.Split(remoteAddr, ":")
		ctx = current.ContextWithTarsCurrent(ctx)
		ok := current.SetClientIPWithContext(ctx, ipPort[0])
		if !ok {
			TLOG.Error("Failed to set context with client ip")
		}
		ok = current.SetClientPortWithContext(ctx, ipPort[1])
		if !ok {
			TLOG.Error("Failed to set context with client port")
		}

		// 执行处理函数，并得到回包
		rsp := h.ts.invoke(ctx, pkg)

		// 向连接中写回包
		if _, err := conn.Write(rsp); err != nil {
			TLOG.Errorf("send pkg to %v failed %v", remoteAddr, err)
		}
	}

	cfg := h.conf
	if cfg.MaxInvoke > 0 { // use goroutine pool
		if h.gpool == nil {
			h.gpool = gpool.NewPool(int(cfg.MaxInvoke), cfg.QueueCap)
		}
		// 向job queue中写入一个数据
		h.gpool.JobQueue <- handler
	} else {
		go handler()
	}
}

// Handle函数在调用Listen被调用
func (h *tcpHandler) Handle() error {
	cfg := h.conf
	// 这里是个无限循环，等待tars
	for !h.ts.isClosed {
		h.lis.SetDeadline(time.Now().Add(cfg.AcceptTimeout)) // set accept timeout
		conn, err := h.lis.AcceptTCP()
		if err != nil {
			if !isNoDataError(err) {
				TLOG.Errorf("Accept error: %v", err)
			} else if conn != nil {
				conn.SetKeepAlive(true)
			}
			continue
		}

		// 每个新连接都会开一个协程
		go func(conn *net.TCPConn) {
			TLOG.Debug("TCP accept:", conn.RemoteAddr())
			atomic.AddInt32(&h.acceptNum, 1)

			//
			conn.SetReadBuffer(cfg.TCPReadBuffer)
			conn.SetWriteBuffer(cfg.TCPWriteBuffer)

			conn.SetNoDelay(cfg.TCPNoDelay)
			h.recv(conn)
			atomic.AddInt32(&h.acceptNum, -1)
		}(conn)
	}
	if h.gpool != nil {
		h.gpool.Release()
	}
	return nil
}

func (h *tcpHandler) recv(conn *net.TCPConn) {
	defer conn.Close()
	cfg := h.conf
	buffer := make([]byte, 1024*4)
	var currBuffer []byte // need a deep copy of buffer
	h.idleTime = time.Now()
	var n int
	var err error

	// 同样是个无限循环，跟Handle()中的原理一样
	for !h.ts.isClosed {
		if cfg.ReadTimeout != 0 {
			conn.SetReadDeadline(time.Now().Add(cfg.ReadTimeout))
		}
		n, err = conn.Read(buffer)

		// 如果发生错误了
		if err != nil {
			if len(currBuffer) == 0 && h.ts.numInvoke == 0 && h.idleTime.Add(cfg.IdleTimeout).Before(time.Now()) {
				return
			}
			h.idleTime = time.Now()
			if isNoDataError(err) {
				continue
			}
			if err == io.EOF {
				TLOG.Debug("connection closed by remote:", conn.RemoteAddr())
			} else {
				TLOG.Error("read packge error:", reflect.TypeOf(err), err)
			}
			return
		}

		currBuffer = append(currBuffer, buffer[:n]...)
		for {
			pkgLen, status := h.ts.svr.ParsePackage(currBuffer)
			if status == PACKAGE_LESS {
				break
			}
			if status == PACKAGE_FULL {
				pkg := make([]byte, pkgLen-4)
				copy(pkg, currBuffer[4:pkgLen])
				currBuffer = currBuffer[pkgLen:]

				// 处理一个完整的数据包
				h.handleConn(conn, pkg)

				if len(currBuffer) > 0 {
					continue
				}
				currBuffer = nil
				break
			}
			TLOG.Errorf("parse packge error %s %v", conn.RemoteAddr(), err)
			return
		}
	}
}
