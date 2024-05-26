/*
Danger from the Deep - Open source submarine simulation
Copyright (C) 2003-2020  Thorsten Jordan, Luis Barrancos and others.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

// Vertex Buffer Object
// (C)+(W) by Thorsten Jordan. See LICENSE

#include "vertexbufferobject.hpp"

#include "OglExt.h"
#include "error.hpp"
#include "log.hpp"
#include "system_interface.hpp"

vertexbufferobject::vertexbufferobject(bool indexbuffer)
    : target(indexbuffer ? GL_ELEMENT_ARRAY_BUFFER : GL_ARRAY_BUFFER)
{
    glGenBuffers(1, &id);
}

vertexbufferobject::~vertexbufferobject()
{
    if (mapped) {
        unmap();
    }
    glDeleteBuffers(1, &id);
}

void vertexbufferobject::init_data(unsigned size_, const void* data, int usage)
{
    size = size_;
    bind();
    glBufferData(target, size, data, usage);
    unbind();
}

void vertexbufferobject::init_sub_data(unsigned offset, unsigned subsize, const void* data) const
{
    bind();
    glBufferSubData(target, offset, subsize, data);
    unbind();
}

void vertexbufferobject::bind() const
{
    glBindBuffer(target, id);
}

void vertexbufferobject::unbind() const
{
    glBindBuffer(target, 0);
}

auto vertexbufferobject::map(int access) -> void*
{
    if (mapped) {
        THROW(error, "vertex buffer object mapped twice");
    }
    bind();
    void* addr = glMapBuffer(target, access);
    if (addr == nullptr) {
        THROW(error, "vertex buffer object mapping failed");
    }
    mapped = true;
    return addr;
}

void vertexbufferobject::unmap()
{
    if (!mapped) {
        THROW(error, "vertex buffer object not mapped before unmap()");
    }
    mapped = false;
    bind(); // FIXME: do we really need this?
    if (glUnmapBuffer(target) != GL_TRUE) {
        log_warning("failed to unmap Vertex Buffer object, data invalid");
    }
    unbind();
}
