
#include "RenderTexture.h"

std::shared_ptr<RenderTexture> RenderTexture::create(ImVec2 resolution)
{
    std::shared_ptr<RenderTexture> ptr = std::make_shared<RenderTexture>();

    if (ptr->init(resolution))
        return ptr;

    return nullptr;
}

bool RenderTexture::init(ImVec2 resolution)
{
    glGenFramebuffers(1, &framebuffer);

    glGenTextures(1, &textureID);
    glGenRenderbuffers(1, &renderbuffer);

    resize(resolution);

    return glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
}

void RenderTexture::resize(ImVec2 resolution)
{
    this->resolution = resolution;

    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

    glBindTexture(GL_TEXTURE_2D, textureID);

    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA,
        static_cast<GLsizei>(resolution.x),
        static_cast<GLsizei>(resolution.y),
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        nullptr
    );

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glFramebufferTexture2D(
        GL_FRAMEBUFFER,
        GL_COLOR_ATTACHMENT0,
        GL_TEXTURE_2D,
        textureID,
        0
    );

    glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer);

    glRenderbufferStorage(
        GL_RENDERBUFFER,
        GL_DEPTH_COMPONENT16,
        static_cast<GLsizei>(resolution.x),
        static_cast<GLsizei>(resolution.y)
    );

    glFramebufferRenderbuffer(
        GL_FRAMEBUFFER,
        GL_DEPTH_ATTACHMENT,
        GL_RENDERBUFFER,
        renderbuffer
    );

    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void RenderTexture::bind()
{
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glViewport(
        0,
        0,
        static_cast<GLsizei>(resolution.x),
        static_cast<GLsizei>(resolution.y)
    );
}

void RenderTexture::unbind()
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void RenderTexture::clear()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}
