# Gesture Recognition Project

This project is developed by [Phakphum Artkaew](https://github.com/PhakphumAdev), [Tianyu Gao](https://github.com/jgnmlxt), and [Luna Liu](https://github.com/LunaXiaoyuLiu)

## Introduction

This project takes in gestures on the x-y plane. One gesture is used as the key gesture, while others are compared with the key to determine their similarities. 

- **Denoising:** A low-pass filter is applied to remove noise.
- **Delay and Distortion Tolerance:** 
  - Dynamic Time Warping (DTW)
  - Cross-correlation 

- The sensitivities are set relatively low. In our tests, even when gestures seemed to match to us, the correlations were pretty low. Despite this, it has good accuracy, sensitivity, and precision, and is functioning for most cases.

