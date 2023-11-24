#!/usr/bin/env bash

ffmpeg -i raw/aws_logo.png -pix_fmt rgb565 -y aws_logo.bmp
ffmpeg -i raw/bear.jpg -pix_fmt rgb565 -y bear.bmp
ffmpeg -i raw/day1.jpg -pix_fmt rgb565 -y day1.bmp
ffmpeg -i raw/employees.jpg -pix_fmt rgb565 -y employees.bmp
ffmpeg -i raw/toy.jpg -pix_fmt rgb565 -y toy.bmp
