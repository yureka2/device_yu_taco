#
# Copyright (C) 2016 The CyanogenMod Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

# Inherit from those products. Most specific first.
$(call inherit-product, $(SRC_TARGET_DIR)/product/core_64_bit.mk)
$(call inherit-product, $(SRC_TARGET_DIR)/product/full_base_telephony.mk)

# Inherit from wt89536 device
$(call inherit-product, device/yu/taco/device.mk)

# Inherit some common CM stuff.
$(call inherit-product, vendor/dot/config/common_full_phone.mk)

# Device identifier. This must come after all inclusions.
PRODUCT_NAME := dot_taco
PRODUCT_DEVICE := taco
PRODUCT_BRAND := YU
PRODUCT_MODEL := taco
PRODUCT_MANUFACTURER := YUREKA2

PRODUCT_GMS_CLIENTID_BASE := android-yu

TARGET_VENDOR := yu

PRODUCT_BUILD_PROP_OVERRIDES += \
    BUILD_FINGERPRINT="YU/YUREKA2/YUREKA2:6.0.1/MMB29M/01112051:user/release-keys" \
    PRIVATE_BUILD_DESC="wt89536-user 6.0.1 MMB29M eng.zhouchao.20170723.172948 release-keys"

