#!/bin/bash

	source ./sdk/environment-setup-aarch64-oe-linux

	mkdir -p tracker/build 
	cd tracker/build
	cmake ..
	make