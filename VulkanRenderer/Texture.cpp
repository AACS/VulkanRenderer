#include "Texture.h"

#include "VulkanTools.h"

Texture::Texture() {};

Texture::~Texture() {
	stbi_image_free(pixels);
};

void Texture::loadFromFile(std::string filename) {
	if (!fileExists(filename)) {
		std::cout << "Could not load texture from " << filename << "File not found" << std::endl;
	}
	int texWidth, texHeight, texChannels;
	this->pixels = stbi_load(filename.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
	this->texImageSize = texWidth * texHeight * 4;

	if (!pixels) {
		throw std::runtime_error("failed to load texture image!");
	}

	this->width = static_cast<uint32_t>(texWidth);
	this->height = static_cast<uint32_t>(texHeight);
	this->mipLevels = static_cast<uint32_t>(1);
};

CubeMap::~CubeMap() {

}

void CubeMap::loadFromFile(std::string filename, std::string fileExt) {
	cubeImages.Front.loadFromFile(filename + "Front" + fileExt);
	cubeImages.Back.loadFromFile(filename + "Back" + fileExt);
	cubeImages.Left.loadFromFile(filename + "Left" + fileExt);
	cubeImages.Right.loadFromFile(filename + "Right" + fileExt);
	cubeImages.Top.loadFromFile(filename + "Top" + fileExt);
	cubeImages.Bottom.loadFromFile(filename + "Bottom" + fileExt);

	this->width = static_cast<uint32_t>(cubeImages.Front.width);
	this->height = static_cast<uint32_t>(cubeImages.Front.height);
	this->mipLevels = static_cast<uint32_t>(cubeImages.Front.mipLevels);

	this->texImageSize = (cubeImages.Front.texImageSize + cubeImages.Back.texImageSize + cubeImages.Top.texImageSize + cubeImages.Bottom.texImageSize + cubeImages.Right.texImageSize + cubeImages.Left.texImageSize);

	stbi_uc* pix = (stbi_uc*)malloc(texImageSize);
	stbi_uc* offset = pix;
	std::memcpy(offset, cubeImages.Front.pixels, cubeImages.Front.texImageSize);

	offset += cubeImages.Front.texImageSize;
	std::memcpy(offset, cubeImages.Back.pixels, cubeImages.Back.texImageSize);

	offset += cubeImages.Back.texImageSize;
	std::memcpy(offset, cubeImages.Top.pixels, cubeImages.Top.texImageSize);

	offset += cubeImages.Top.texImageSize;
	std::memcpy(offset, cubeImages.Bottom.pixels, cubeImages.Bottom.texImageSize);

	offset += cubeImages.Bottom.texImageSize;
	std::memcpy(offset, cubeImages.Left.pixels, cubeImages.Left.texImageSize);

	offset += cubeImages.Left.texImageSize;
	std::memcpy(offset, cubeImages.Right.pixels, cubeImages.Right.texImageSize);

	this->pixels = pix;
};
