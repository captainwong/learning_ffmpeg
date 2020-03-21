#pragma once

struct AVFrame;

struct IVideoCall {
	virtual void doInit(int width, int height) = 0;
	virtual void doRepaint(AVFrame* frame) = 0;
};
