#!/bin/bash
rsync -avz --exclude 'build/' --exclude '.git/' ~/Development/hue_randomwhite/ mikkanen@Raspberry5.local:~/Development/hue_randomwhite/
# ssh mikkanen@Raspberry5.local "cd ~/Development/hue_randomwhite/build && make -j$(nproc)"
