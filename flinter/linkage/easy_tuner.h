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

#ifndef FLINTER_LINKAGE_EASY_TUNER_H
#define FLINTER_LINKAGE_EASY_TUNER_H

namespace flinter {

class EasyTuner {
public:
    virtual ~EasyTuner() {}

    /// Called within job threads right after they start.
    virtual bool OnJobThreadInitialize()
    {
        return true;
    }

    /// Called within job threads right before they terminate.
    virtual void OnJobThreadShutdown() {}

    /// Called within I/O threads right after they start.
    virtual bool OnIoThreadInitialize()
    {
        return true;
    }

    /// Called within I/O threads right before they terminate.
    virtual void OnIoThreadShutdown() {}

}; // class EasyTuner

} // namespace flinter

#endif // FLINTER_LINKAGE_EASY_TUNER_H
