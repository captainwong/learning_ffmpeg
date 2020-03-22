#include "XVideoWidget.h"
#include <QDebug>
#include <QTimer>

#pragma warning(disable:4819)

extern "C" {
#include <libavutil/frame.h>
}

static constexpr int vertexLocation = 3;
static constexpr int textureLocation = 4;

static constexpr const char* vertexString = R"(
attribute vec4 vertexIn;
attribute vec2 textureIn;
varying vec2 textureOut;
void main(void) {
	gl_Position = vertexIn;
	textureOut = textureIn;
}
)";

static constexpr const char* fragmentString = R"(
varying vec2 textureOut;
uniform sampler2D texY;
uniform sampler2D texU;
uniform sampler2D texV;
void main(void) {
	vec3 yuv;
	vec3 rgb;
	yuv.x = texture2D(texY, textureOut).r;
	yuv.y = texture2D(texU, textureOut).r - 0.5;
	yuv.z = texture2D(texV, textureOut).r - 0.5;
	rgb = mat3(1.0, 1.0, 1.0,
			0.0, -0.39465, 2.03211,
			1.13983, -0.58060, 0.0) * yuv;
	gl_FragColor = vec4(rgb, 1.0);
}
)";


XVideoWidget::XVideoWidget(QWidget* parent)
	: QOpenGLWidget(parent)
	, QOpenGLFunctions()
	, IVideoCall()
{

}

XVideoWidget::~XVideoWidget()
{
}

void XVideoWidget::doInit(int width, int height)
{
	std::lock_guard<std::mutex> lg(mutex_);

	width_ = width;
	height_ = height;
	if (data_[0]) { delete[] data_[0]; }
	if (data_[1]) { delete[] data_[1]; }
	if (data_[2]) { delete[] data_[2]; }
	data_[0] = new unsigned char[width * height];		// Y
	data_[1] = new unsigned char[width * height / 4];	// U
	data_[2] = new unsigned char[width * height / 4];	// V

	if (textures_[0]) { glDeleteTextures(3, textures_); }
	glGenTextures(3, textures_);

	// Y
	glBindTexture(GL_TEXTURE_2D, textures_[0]);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);

	// U
	glBindTexture(GL_TEXTURE_2D, textures_[1]);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width / 2, height / 2, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);

	// V
	glBindTexture(GL_TEXTURE_2D, textures_[2]);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width / 2, height / 2, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
}

void XVideoWidget::doRepaint(AVFrame* frame)
{
	if (!frame) { return; }

	{
		std::lock_guard<std::mutex> lg(mutex_);

		if (!data_[0] || width_ * height_ == 0 || frame->width != width_ || frame->height != height_) {
			av_frame_free(&frame);
			return;
		}

		if (width_ == frame->linesize[0]) {
			memcpy(data_[0], frame->data[0], width_ * height_);		// Y
			memcpy(data_[1], frame->data[1], width_ * height_ / 4);	// U
			memcpy(data_[2], frame->data[2], width_ * height_ / 4);	// V
		} else {
			for (int i = 0; i < height_; i++) { // Y
				memcpy(data_[0] + width_ * i, frame->data[0] + frame->linesize[0] * i, width_);
			}
			for (int i = 0; i < height_ / 2; i++) { // U
				memcpy(data_[1] + width_ / 2 * i, frame->data[1] + frame->linesize[1] * i, width_);
			}
			for (int i = 0; i < height_ / 2; i++) { // V
				memcpy(data_[2] + width_ / 2 * i, frame->data[2] + frame->linesize[2] * i, width_);
			}
		}
	}

	av_frame_free(&frame);
	update();
}

void XVideoWidget::initializeGL()
{
	qDebug() << "initializeGL";
	std::lock_guard<std::mutex> lg(mutex_);

	initializeOpenGLFunctions();
	qDebug() << "add vertex shader" << program_.addShaderFromSourceCode(QGLShader::Vertex, vertexString);
	qDebug() << "add texture shader" << program_.addShaderFromSourceCode(QGLShader::Fragment, fragmentString);

	// 设置顶点坐标变量
	program_.bindAttributeLocation("vertexIn", vertexLocation);
	// 设置材质坐标变量
	program_.bindAttributeLocation("textureIn", textureLocation);

	qDebug() << "link" << program_.link();
	qDebug() << "bind" << program_.bind();

	static constexpr GLfloat vertex[] = {
		-1.0f, -1.0f,
		1.0f, -1.0f,
		-1.0f, 1.0f,
		1.0f, 1.0f,
	};

	static constexpr GLfloat texture[] = {
		0.0f, 1.0f,
		1.0f, 1.0f,
		0.0f, 0.0f,
		1.0f, 0.0f,
	};

	glVertexAttribPointer(vertexLocation, 2, GL_FLOAT, 0, 0, vertex);
	glEnableVertexAttribArray(vertexLocation);

	glVertexAttribPointer(textureLocation, 2, GL_FLOAT, 0, 0, texture);
	glEnableVertexAttribArray(textureLocation);

	// 从shader获取材质
	uniforms_[0] = program_.uniformLocation("texY");
	uniforms_[1] = program_.uniformLocation("texU");
	uniforms_[2] = program_.uniformLocation("texV");
}

void XVideoWidget::paintGL()
{
	glActiveTexture(GL_TEXTURE0); // 0层
	glBindTexture(GL_TEXTURE_2D, textures_[0]); // 绑定到Y材质
	// 修改材质内容
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width_, height_, GL_RED, GL_UNSIGNED_BYTE, data_[0]);
	// 与shader uniform遍历关联
	glUniform1i(uniforms_[0], 0);

	glActiveTexture(GL_TEXTURE0 + 1); // 1层
	glBindTexture(GL_TEXTURE_2D, textures_[1]); // 绑定到U材质
	// 修改材质内容
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width_ / 2, height_ / 2, GL_RED, GL_UNSIGNED_BYTE, data_[1]);
	// 与shader uniform遍历关联
	glUniform1i(uniforms_[1], 1);

	glActiveTexture(GL_TEXTURE0 + 2); // 2层
	glBindTexture(GL_TEXTURE_2D, textures_[2]); // 绑定到V材质
	// 修改材质内容
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width_ / 2, height_ / 2, GL_RED, GL_UNSIGNED_BYTE, data_[2]);
	// 与shader uniform遍历关联
	glUniform1i(uniforms_[2], 2);

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

void XVideoWidget::resizeGL(int width, int height)
{
	std::lock_guard<std::mutex> lg(mutex_);
	qDebug() << "resizeGL " << width << ":" << height;
}
