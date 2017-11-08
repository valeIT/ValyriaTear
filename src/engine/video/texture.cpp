///////////////////////////////////////////////////////////////////////////////
//            Copyright (C) 2004-2011 by The Allacrost Project
//            Copyright (C) 2012-2016 by Bertram (Valyria Tear)
//                         All Rights Reserved
//
// This code is licensed under the GNU GPL version 2. It is free software
// and you may modify it and/or redistribute it under the terms of this license.
// See http://www.gnu.org/copyleft/gpl.html for details.
///////////////////////////////////////////////////////////////////////////////

/** ****************************************************************************
*** \file    tex_mgmt.h
*** \author  Raj Sharma, roos@allacrost.org
*** \author  Yohann Ferreira, yohann ferreira orange fr
*** \brief   Source file for texture management code
*** ***************************************************************************/

#include "common/include_pch.h"
#include "texture.h"

#include "video.h"

using namespace vt_utils;

namespace vt_video
{

namespace private_video
{

// -----------------------------------------------------------------------------
// TexSheet class
// -----------------------------------------------------------------------------

TexSheet::TexSheet(uint32_t sheet_width, uint32_t sheet_height, GLuint sheet_id, TexSheetType sheet_type, bool sheet_static) :
    width(sheet_width),
    height(sheet_height),
    tex_id(sheet_id),
    type(sheet_type),
    is_static(sheet_static),
    smoothed(false),
    loaded(true)
{
    Smooth();
}

TexSheet::~TexSheet()
{
    // Unload the OpenGL texture from memory.
    TextureManager->_DeleteTexture(tex_id);
}

bool TexSheet::Unload()
{
    if (loaded == false) {
        IF_PRINT_WARNING(VIDEO_DEBUG) << "attempted to unload an already unloaded texture sheet" << std::endl;
        return false;
    }

    TextureManager->_DeleteTexture(tex_id);
    tex_id = INVALID_TEXTURE_ID;
    loaded = false;
    return true;
}

bool TexSheet::Reload()
{
    if (loaded) {
        if(VIDEO_DEBUG)
            IF_PRINT_WARNING(VIDEO_DEBUG) << "attempted to load an already loaded texture sheet" << std::endl;
        return false;
    }

    // Create new OpenGL texture.
    GLuint id = TextureManager->_CreateBlankGLTexture(width, height);

    if (id == INVALID_TEXTURE_ID) {
        PRINT_ERROR << "call to TextureController::_CreateBlankGLTexture() failed" << std::endl;
        return false;
    }

    tex_id = id;

    // Restore texture smoothing if applied.
    bool was_smoothed = smoothed;
    smoothed = false;
    Smooth(was_smoothed);

    // Reload all of the images that belong to this texture
    if(TextureManager->_ReloadImagesToSheet(this) == false) {
        PRINT_ERROR << "call to TextureController::_ReloadImagesToSheet() failed" << std::endl;
        return false;
    }

    loaded = true;
    return true;
}

bool TexSheet::CopyRect(int32_t x, int32_t y, ImageMemory& data)
{
    TextureManager->_BindTexture(tex_id);

    data.GlTexSubImage(x, y);

    if(VideoManager->CheckGLError()) {
        IF_PRINT_WARNING(VIDEO_DEBUG) << "an OpenGL error occured: " << VideoManager->CreateGLErrorString() << std::endl;
        return false;
    }

    return true;
}

bool TexSheet::CopyScreenRect(int32_t x, int32_t y, const ScreenRect &screen_rect)
{
    TextureManager->_BindTexture(tex_id);

    glCopyTexSubImage2D(
        GL_TEXTURE_2D, // target
        0, // level
        x, // x offset within tex sheet
        y, // y offset within tex sheet
        screen_rect.left, // left starting pixel of the screen to copy
        screen_rect.top, // top starting pixel of the screen to copy
        screen_rect.width, // width in pixels of image
        screen_rect.height // height in pixels of image
    );

    if(VideoManager->CheckGLError()) {
        IF_PRINT_WARNING(VIDEO_DEBUG) << "an OpenGL error occured: " << VideoManager->CreateGLErrorString() << std::endl;
        return false;
    }

    return true;
}

void TexSheet::Smooth(bool flag)
{
    // If setting has changed, set the appropriate filtering
    if(smoothed != flag) {
        smoothed = flag;
        GLenum filtering_type = smoothed ? GL_LINEAR : GL_NEAREST;

        TextureManager->_BindTexture(tex_id);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filtering_type);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filtering_type);
    }
}

void TexSheet::DEBUG_Draw() const
{
    // The vertex positions.
    float vertex_positions[] =
    {
        1.0f, 1.0f, 0.0f, // Vertex One.
        0.0f, 1.0f, 0.0f, // Vertex Two.
        0.0f, 0.0f, 0.0f, // Vertex Three.
        1.0f, 0.0f, 0.0f  // Vertex Four.
    };

    // The vertex texture coordinates.
    float vertex_texture_coordinates[] =
    {
        0.0f, 1.0f, // Vertex One.
        1.0f, 1.0f, // Vertex Two.
        1.0f, 0.0f, // Vertex Three.
        0.0f, 0.0f  // Vertex Four.
    };

    // The vertex colors.
    float vertex_colors[] =
    {
        1.0f, 1.0f, 1.0f, 1.0f, // Vertex One.
        1.0f, 1.0f, 1.0f, 1.0f, // Vertex Two.
        1.0f, 1.0f, 1.0f, 1.0f, // Vertex Three.
        1.0f, 1.0f, 1.0f, 1.0f  // Vertex Four.
    };

    // Enable texturing and bind the texture.
    VideoManager->DisableBlending();
    VideoManager->EnableTexture2D();
    TextureManager->_BindTexture(tex_id);

    // Load the solid shader program.
    gl::ShaderProgram* shader_program = VideoManager->LoadShaderProgram(gl::shader_programs::Solid);
    assert(shader_program != nullptr);

    // Draw a black background.
    VideoManager->DrawSprite(shader_program, vertex_positions, vertex_texture_coordinates, vertex_colors, ::vt_video::Color::black);

    // Unload the shader program.
    VideoManager->UnloadShaderProgram();

    // Load the sprite shader program.
    shader_program = VideoManager->LoadShaderProgram(gl::shader_programs::Sprite);
    assert(shader_program != nullptr);

    // Draw the image.
    VideoManager->DrawSprite(shader_program, vertex_positions, vertex_texture_coordinates, vertex_colors);

    // Unload the shader program.
    VideoManager->UnloadShaderProgram();
}

// -----------------------------------------------------------------------------
// FixedTexSheet class
// -----------------------------------------------------------------------------

FixedTexSheet::FixedTexSheet(int32_t sheet_width, int32_t sheet_height, GLuint sheet_id, TexSheetType sheet_type, bool sheet_static, int32_t img_width, int32_t img_height) :
    TexSheet(sheet_width, sheet_height, sheet_id, sheet_type, sheet_static),
    _texture_width(img_width),
    _texture_height(img_height)
{
    // Set all the dimensions
    _block_width  = width / _texture_width;
    _block_height = height / _texture_height;

    // Allocate the blocks array
    int32_t num_blocks = _block_width * _block_height;
    _blocks = new FixedTexNode[num_blocks];

    // Construct the linked list of open blocks
    _open_list_head = &_blocks[0];
    _open_list_tail = &_blocks[num_blocks - 1];

    for(int32_t i = 0; i < num_blocks - 1; i++) {
        _blocks[i].image = nullptr;
        _blocks[i].next = &_blocks[i + 1];
        _blocks[i].block_index = i;
    }

    _open_list_tail->image = nullptr;
    _open_list_tail->next = nullptr;
    _open_list_tail->block_index = num_blocks - 1;
}

FixedTexSheet::~FixedTexSheet()
{
    if (GetNumberTextures() != 0)
        IF_PRINT_WARNING(VIDEO_DEBUG) << "texture sheet being deleted when it has a non-zero allocated texture count: " << GetNumberTextures() << std::endl;

    if (_blocks != nullptr) {
        delete[] _blocks;
        _blocks = nullptr;
    }
}

bool FixedTexSheet::AddTexture(BaseTexture *img, ImageMemory &data)
{
    if (InsertTexture(img) == false)
        return false;

    // Copy the pixel data for the texture over.
    if (CopyRect(img->x, img->y, data) == false) {
        IF_PRINT_WARNING(VIDEO_DEBUG) << "VIDEO ERROR: CopyRect() failed in TexSheet::AddImage()!" << std::endl;
        return false;
    }

    return true;
}

bool FixedTexSheet::InsertTexture(BaseTexture *img)
{
    if(img == nullptr) {
        IF_PRINT_WARNING(VIDEO_DEBUG) << "nullptr pointer was given as function argument" << std::endl;
        return false;
    }

    // Retrieve the node from the head of the list to use for this texture
    FixedTexNode *node = _RemoveOpenNode();
    if(node == nullptr)  // This condition indicates that there are no remaining free nodes on the open list
        return false;

    // Check if there's already an image allocated at this block (an image was freed earlier, but not removed)
    // If so, we must now remove it from memory
    if(node->image != nullptr) {
        // TODO: TextureManager needs to have the image element removed from its map containers
        node->image = nullptr;
    }

    // Calculate the texture's pixel coordinates in the sheet given this node's block index
    img->x = _texture_width * (node->block_index % _block_width);
    img->y = _texture_height * (node->block_index / _block_width);

    // Calculate the u,v coordinates
    float sheet_width = static_cast<float>(width);
    float sheet_height = static_cast<float>(height);

    img->u1 = static_cast<float>(img->x + 0.5f) / sheet_width;
    img->u2 = static_cast<float>(img->x + img->width - 0.5f) / sheet_width;
    img->v1 = static_cast<float>(img->y + 0.5f) / sheet_height;
    img->v2 = static_cast<float>(img->y + img->height - 0.5f) / sheet_height;

    img->texture_sheet = this;
    node->image = img;
    return true;
} // bool FixedTexSheet::InsertTexture(BaseTexture* img)



void FixedTexSheet::RemoveTexture(BaseTexture *img)
{
    if(img == nullptr) {
        IF_PRINT_WARNING(VIDEO_DEBUG) << "nullptr pointer was given as function argument" << std::endl;
        return;
    }

    int32_t block_index = _CalculateBlockIndex(img);

    // Check to make sure the block is actually owned by this image
    if(_blocks[block_index].image != img) {
        IF_PRINT_WARNING(VIDEO_DEBUG) << "tried to remove a fixed block not owned by the image" << std::endl;
        return;
    }

    _blocks[block_index].image = nullptr;
    _AddOpenNode(&_blocks[block_index]);
}



void FixedTexSheet::FreeTexture(BaseTexture *img)
{
    if(img == nullptr) {
        IF_PRINT_WARNING(VIDEO_DEBUG) << "nullptr pointer was given as function argument" << std::endl;
        return;
    }

    int32_t block_index = _CalculateBlockIndex(img);

    // Check to make sure the block is actually owned by this image
    if(_blocks[block_index].image != img) {
        IF_PRINT_WARNING(VIDEO_DEBUG) << "tried to remove a fixed block not owned by the image" << std::endl;
        return;
    }

    // Unliked the RemoveTexture call, we do not set the block's image to nullptr here
    _AddOpenNode(&_blocks[block_index]);
}



void FixedTexSheet::RestoreTexture(BaseTexture *img)
{
    if(img == nullptr) {
        IF_PRINT_WARNING(VIDEO_DEBUG) << "nullptr pointer was given as function argument" << std::endl;
        return;
    }

    // Go through the list of open nodes and find the node with this image
    FixedTexNode *last = nullptr;
    FixedTexNode *now = _open_list_head;

    while(now != nullptr) {
        // If we found the texture, update the list so that the containing node temporarily becomes
        // the head of the open list, then remove that node from the list head
        if(now->image == img) {
            if(last != nullptr) {
                last = now->next;
                now->next = _open_list_head;
                _open_list_head = now;
            }

            _RemoveOpenNode();
            return;
        }

        last = now;
        now = now->next;
    }

    IF_PRINT_WARNING(VIDEO_DEBUG) << "failed to restore, texture was not found in open list" << std::endl;
}



uint32_t FixedTexSheet::GetNumberTextures()
{
    uint32_t num_blocks = 0;

    for(int32_t i = 0; i < _block_width * _block_height; i++) {
        if(_blocks[i].image != nullptr) {
            num_blocks++;
        }
    }

    return num_blocks;
}



int32_t FixedTexSheet::_CalculateBlockIndex(BaseTexture *img)
{
    int32_t block_x = img->x / _texture_width;
    int32_t block_y = img->y / _texture_height;

    return (block_x + _block_width * block_y);
}



void FixedTexSheet::_AddOpenNode(FixedTexNode *node)
{
    if(_open_list_tail != nullptr) {
        _open_list_tail->next = node;
        _open_list_tail = node;
        _open_list_tail->next = nullptr;
    } else {
        _open_list_head = node;
        _open_list_tail = node;
        _open_list_tail->next = nullptr;
    }
}



FixedTexNode *FixedTexSheet::_RemoveOpenNode()
{
    if(_open_list_head == nullptr)
        return nullptr;

    FixedTexNode *node = _open_list_head;
    _open_list_head = _open_list_head->next;
    node->next = nullptr;

    // This condition means we just removed the last open block, so set the tail pointer to nullptr as well
    if(_open_list_head == nullptr) {
        _open_list_tail = nullptr;
    }

    return node;
}

// -----------------------------------------------------------------------------
// VariableTexSheet class
// -----------------------------------------------------------------------------

VariableTexSheet::VariableTexSheet(int32_t sheet_width, int32_t sheet_height, GLuint sheet_id, TexSheetType sheet_type, bool sheet_static) :
    TexSheet(sheet_width, sheet_height, sheet_id, sheet_type, sheet_static)
{
    _block_width = width / 16;
    _block_height = height / 16;
    _blocks = new VariableTexNode[_block_width * _block_height];
}

VariableTexSheet::~VariableTexSheet()
{
    if (GetNumberTextures() != 0)
        IF_PRINT_WARNING(VIDEO_DEBUG) << "texture sheet being deleted when it has a non-zero allocated texture count: " << GetNumberTextures() << std::endl;

    if (_blocks != nullptr) {
        delete [] _blocks;
        _blocks = nullptr;
    }
}

bool VariableTexSheet::AddTexture(BaseTexture *img, ImageMemory &data)
{
    if(InsertTexture(img) == false)
        return false;

    // Copy the pixel data for the texture over
    if(CopyRect(img->x, img->y, data) == false) {
        IF_PRINT_WARNING(VIDEO_DEBUG) << "VIDEO ERROR: CopyRect() failed in TexSheet::AddImage()!" << std::endl;
        return false;
    }

    return true;
}



bool VariableTexSheet::InsertTexture(BaseTexture *img)
{
    if(img == nullptr) {
        IF_PRINT_WARNING(VIDEO_DEBUG) << "nullptr pointer was given as function argument" << std::endl;
        return false;
    }

    // Don't allow insertions into a texture sheet containing a texture larger than 512x512.
    // Texture sheets with this property may only be used by one texture at a time
    if(_block_width > 32 || _block_height > 32) {  // 32 blocks == 512 pixels
        if(_blocks[0].free_image == false)
            return false;
    }

    // Attempt to find an open region in the texture sheet to fit this texture
    int32_t block_x = -1, block_y = -1;
    int32_t w = (img->width + 15) / 16;
    int32_t h = (img->height + 15) / 16;

    // This is a brute force algorithm to try and find space to allocate the texture.
    // If this becomes a bottleneck, we may wish to use a more intellegent algorithm here.
    bool continue_search = true;
    for(int32_t y = 0; y < _block_height - h + 1 && continue_search; y++) {
        for(int32_t x = 0; x < _block_width - w + 1; x++) {
            int32_t furthest_blocker = -1;

            bool continue_neighbor_search = true;
            for(int32_t dy = 0; dy < h && continue_neighbor_search; dy++) {
                for(int32_t dx = 0; dx < w; dx++) {
                    if(_blocks[(x + dx) + ((y + dy) * _block_width)].free_image == false) {
                        furthest_blocker = x + dx;
                        continue_neighbor_search = false;
                        break;
                    }
                }
            }

            if(furthest_blocker == -1) {
                block_x = x;
                block_y = y;
                continue_search = false;
                break;
            }
        }
    }

    // If either of these conditions is true, it means we were unable to allocate enough space to insert this texture
    if(block_x == -1 || block_y == -1)
        return false;

    // Go through each block that is to be occupied by the new texture and set its properties
    for(int32_t y = block_y; y < block_y + h; y++) {
        for(int32_t x = block_x; x < block_x + w; x++) {
            int32_t index = x + (y * _block_width);

            // If the texture pointer for the block is not nullptr, this means it contains a freed texture.
            // Now we must remove that texture entirely since we are overwriting at least one of its blocks.
            if(_blocks[index].image) {
                RemoveTexture(_blocks[index].image);
            }

            _blocks[index].free_image = false;
            _blocks[index].image = img;
        }
    }

    // Calculate the pixel and uv coordinates for the newly inserted texture
    img->x = block_x * 16;
    img->y = block_y * 16;

    float sheet_width = static_cast<float>(width);
    float sheet_height = static_cast<float>(height);

    img->u1 = static_cast<float>(img->x + 0.5f) / sheet_width;
    img->u2 = static_cast<float>(img->x + img->width - 0.5f) / sheet_width;
    img->v1 = static_cast<float>(img->y + 0.5f) / sheet_height;
    img->v2 = static_cast<float>(img->y + img->height - 0.5f) / sheet_height;

    img->texture_sheet = this;
    _textures.insert(img);

    return true;
} // bool VariableTexSheet::InsertTexture(BaseTexture* img)



void VariableTexSheet::RemoveTexture(BaseTexture *img)
{
    _SetBlockProperties(img, nullptr, true);
    _textures.erase(img);
}



void VariableTexSheet::_SetBlockProperties(BaseTexture *tex, BaseTexture *new_tex, bool free)
{
    if(tex == nullptr) {
        IF_PRINT_WARNING(VIDEO_DEBUG) << "nullptr pointer was given as function argument" << std::endl;
        return;
    }

    if(_textures.find(tex) == _textures.end()) {
        IF_PRINT_WARNING(VIDEO_DEBUG) << "texture pointer argument was not contained within this texture sheet" << std::endl;
    }

    // Calculate upper-left corner in blocks
    int32_t block_x = tex->x / 16;
    int32_t block_y = tex->y / 16;

    // Calculate width and height in blocks
    int32_t w = (tex->width  + 15) / 16;
    int32_t h = (tex->height + 15) / 16;

    for(int32_t y = block_y; y < block_y + h; y++) {
        for(int32_t x = block_x; x < block_x + w; x++) {
            int32_t index = x + y * _block_width;
            if(_blocks[index].image == tex) {
                _blocks[index].free_image = free;
                _blocks[index].image = new_tex;
            }
        }
    }
}

} // namespace private_video

} // namespace vt_video
