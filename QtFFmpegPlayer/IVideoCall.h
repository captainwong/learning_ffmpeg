#pragma once

struct AVFrame;

namespace jlib {
namespace qt {
namespace xplayer {

struct IVideoCall {
	virtual void doInit(int width, int height) = 0;
	virtual void doRepaint(AVFrame* frame) = 0;
};

}
}
}
