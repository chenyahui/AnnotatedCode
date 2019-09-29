/* Copyright 2014 yiyuanzhong@gmail.com (Yiyuan Zhong)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __FACTORY_H
#define __FACTORY_H

namespace flinter {

template <class T>
class Factory {
public:
    Factory() {}
    virtual ~Factory() {}
    virtual T *Create() const = 0;

}; // class Factory

template <class T, class Impl = T>
class FactoryDirect : public Factory<T> {
public:
    FactoryDirect() {}
    virtual ~FactoryDirect() {}
    virtual T *Create() const
    {
        return new Impl;
    }

}; // class FactoryDirect

template <class T, class Impl = T>
class FactoryIndirect : public Factory<T> {
public:
    FactoryIndirect() {}
    virtual ~FactoryIndirect() {}
    virtual T *Create() const
    {
        return Impl::CreateInstance();
    }
}; // class FactoryIndirect

} // namespace flinter

#endif // __FACTORY_H
