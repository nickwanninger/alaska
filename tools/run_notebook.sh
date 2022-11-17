#!/usr/bin/env bash


if [ ! -d .venv ]; then
	virtualenv -p /usr/bin/python3 .venv
	source .venv/bin/activate

	pip3 install notebook numpy seaborn matplotlib
else
	source .venv/bin/activate
fi


jupyter notebook . --no-browser
