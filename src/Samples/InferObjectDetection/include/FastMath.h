/*
 * Copyright (c) 2020.Huawei Technologies Co., Ltd. All rights reserved.
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

#ifndef FASTMATH_H
#define FASTMATH_H

#include <cmath>
#include <memory>

/*
Utilize quantization and look up table to accelate exp operation
*/
class FastMath {
public:
    FastMath() 
    {
        for (auto i = 0; i < maskLen_; i++) {
            negCoef_[0][i] = std::exp(-float(i) / quantValue_);
            negCoef_[1][i] = std::exp(-float(i) * maskLen_ / quantValue_);
            posCoef_[0][i] = std::exp(float(i) / quantValue_);
            posCoef_[1][i] = std::exp(float(i) * maskLen_ / quantValue_);
        }
    }
    ~FastMath() {}
    inline float FExp(const float x) 
    {
        int quantX = std::max(std::min(x, float(quantBound_)), -float(quantBound_)) * quantValue_;
        float expx;
        if (quantX & 0x80000000) {
            expx = negCoef_[0][((~quantX + 0x00000001)) & maskValue_] * negCoef_[1][((~quantX + 0x00000001) >> maskBits_) & maskValue_];
        } else {
            expx = posCoef_[0][(quantX) & maskValue_] * posCoef_[1][(quantX >> maskBits_) & maskValue_];
        }
        return expx;
    }
    inline float Sigmoid(float x) 
    {
        return 1.0f / (1.0f + FExp(-x));
    }

private:
    static const int maskBits_ = 12; // 常量定义说明
    static const int maskLen_ = (1 << maskBits_);
    static const int maskValue_ = maskLen_ - 1;
    static const int quantBits_ = 16;
    static const int quantValue_ = (1 << quantBits_);
    static const int quantBound_ = (1 << (2 * maskBits_ - quantBits_)) - 1;
    float negCoef_[2][maskLen_];
    float posCoef_[2][maskLen_];
};

namespace fastmath {
    static FastMath fastMath;
    inline float exp(const float x)
    {
        return fastMath.FExp(x);
    }
    inline float sigmoid(float x)
    {
        return fastMath.Sigmoid(x);
    }
}

#endif