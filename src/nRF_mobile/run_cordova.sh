#!/bin/sh
cd www/js
browserify index.js -o bundle.js
cd ../..
cordova run
