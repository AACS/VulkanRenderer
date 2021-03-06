#include "InternalGraph.h"

#include "../core/Logger.h"

#include "../../third-party/stb_image/stb_image.h"


namespace InternalGraph {

	template<typename T>
	static const float BilinearImageSample2D(const NoiseImage2D<T>& noiseImage, const float x, const float z) {
		const int cellScale = noiseImage.GetImageWidth() - 1; // cellsWide - 1;

		const float xScaled = x * (float)cellScale;
		const float zScaled = z * (float)cellScale;

		const int realX = (int)xScaled;
		const int realZ = (int)zScaled;

		const int realXPlus1 = (int)glm::clamp(xScaled + 1, 0.0f, (float)cellScale); //make sure its not greater than the image size
		const int realZPlus1 = (int)glm::clamp(zScaled + 1, 0.0f, (float)cellScale);

		const float UL = noiseImage.BoundedLookUp(realX, realZ);
		const float UR = noiseImage.BoundedLookUp(realX, realZPlus1);
		const float DL = noiseImage.BoundedLookUp(realXPlus1, realZ);
		const float DR = noiseImage.BoundedLookUp(realXPlus1, realZPlus1);

		if (realX == realXPlus1 && realZ == realZPlus1) {
			return UL;
		}
		else if (realX == realXPlus1) {
			return (UL * ((float)realZPlus1 - zScaled) + UR * (zScaled - (float)realZ)) / ((float)realZPlus1 - (float)realZ);
		}
		else if (realZ == realZPlus1) {
			return (UL * ((float)realXPlus1 - xScaled) + DL * (xScaled - (float)realX)) / ((float)realXPlus1 - (float)realX);
		}
		else {

			return (
				UL * ((float)realXPlus1 - xScaled)* ((float)realZPlus1 - zScaled)
				+ DL * (xScaled - (float)realX)		* ((float)realZPlus1 - zScaled)
				+ UR * ((float)realXPlus1 - xScaled)* (zScaled - (float)realZ)
				+ DR * (xScaled - (float)realX)		* (zScaled - (float)realZ)
				)
				/ (((float)realXPlus1 - (float)realX) * ((float)realZPlus1 - (float)realZ));
		}
	}

	//No allocation constructor, meant for images that are externally created (FastNoiseSIMD)
	template<typename T>
	NoiseImage2D<T>::NoiseImage2D() : width(0), isExternallyAllocated(true) {}

	//Preallocates based on width, mainly meant for output images
	template<typename T>
	NoiseImage2D<T>::NoiseImage2D(int width) : width(width), isExternallyAllocated(false) {

		data = std::vector<T>(width*width);
	}

	template<typename T>
	NoiseImage2D<T>::~NoiseImage2D() {
		if (!isExternallyAllocated && image != nullptr)
			free(image);
		//Log::Error << "Was this image already freed?\n";
	}

	template<typename T>
	T* NoiseImage2D<T>::GetImageData() {
		if (isExternallyAllocated)
			return image;
		else
			return data.data();
	}

	template<typename T>
	void NoiseImage2D<T>::SetImageData(int width, T* data) {
		this->width = width;
		image = data;
	}

	template<typename T>
	const int NoiseImage2D<T>::GetSize() const {
		return width * width;
	}

	template<typename T>
	const size_t NoiseImage2D<T>::GetSizeBytes() const {
		return GetSize() * 4;
	}

	template<typename T>
	const int NoiseImage2D<T>::GetImageWidth() const {
		return width;
	}

	//Changes the width then frees and allocates new image 
	template<typename T>
	void NoiseImage2D<T>::SetWidth(int width) {
		this->width = width;
	}

	template<typename T>
	const T NoiseImage2D<T>::LookUp(int x, int z) const {
		if (isExternallyAllocated)
			return image[x * width + z];
		else
			return data[x * width + z];
	}

	template<typename T>
	const T NoiseImage2D<T>::BoundedLookUp(int x, int z) const {
		if (x >= 0 && x < width && z >= 0 && z < width) {
			if (isExternallyAllocated)
				return image[x * width + z];
			else
				return data[x * width + z];
		}
		else {
			throw new std::runtime_error("out of bounds");
		}
		return -1;
	}

	template<typename T>
	void NoiseImage2D<T>::SetPixelValue(int x, int z, T value) {


		if (x >= 0 && x < width && z >= 0 && z < width)
			data[x * width + z] = value;
		//	image[x * width + z] = value;
		else
			throw new std::runtime_error("out of bounds");
	}

	InputLink::InputLink() : value(-1.0f) {}
	InputLink::InputLink(float in) : value(in) {}
	InputLink::InputLink(int in) : value(in) {}
	InputLink::InputLink(glm::vec2 in) : value(in) {}
	InputLink::InputLink(glm::vec3 in) : value(in) {}
	InputLink::InputLink(glm::vec4 in) : value(in) {}


	void InputLink::SetInputNode(NodeID id) {
		handle.id = id;
		hasInputNode = true;
	}

	void InputLink::ResetInputNode() {
		handle.id = -1;
		hasInputNode = false;
		handle.handle = nullptr;
	}

	NodeID InputLink::GetInputNode() const {
		return handle.id;
	}

	bool InputLink::HasInputNode() const {
		return hasInputNode;
	}

	void InputLink::SetInputNodePointer(Node* node) {
		hasInputNode = true;
		handle.handle = node;
	}

	void InputLink::ResetInputNodePointer() {
		hasInputNode = false;
		handle.handle = nullptr;
	}

	void InputLink::SetDataValue(LinkTypeVariants data) {
		value = data;
	}

	LinkTypeVariants InputLink::GetValue() const {
		return value;
	}

	LinkTypeVariants InputLink::GetValue(const int x, const int z) const {

		if (hasInputNode)
			return handle.handle->GetValue(x, z);
		else {

			return value;

		}
	}

	void AddNodeInputLinks(std::vector<InputLink>& links, std::vector<LinkType> types) {
		for (auto& t : types) {
			switch (t) {
			case LinkType::Float:
				links.push_back(0.0f);
				break;
			case LinkType::Int:
				links.push_back(0);
				break;
			case LinkType::Vec2:
				links.push_back(glm::vec2(0));
				break;
			case LinkType::Vec3:
				links.push_back(glm::vec3(0));
				break;
			case LinkType::Vec4:
				links.push_back(glm::vec4(0));
				break;
			default:
				break;
			}
		}

	}

	Node::Node(NodeType in_type) : nodeType(in_type) {
		outputType = LinkType::Float;

		switch (nodeType)
		{
		case InternalGraph::NodeType::Output:
			AddNodeInputLinks(inputLinks,
				{ LinkType::Float, LinkType::Vec4, LinkType::Int, LinkType::Int, LinkType::Int, LinkType::Int });
			break;

		case InternalGraph::NodeType::Addition:
		case InternalGraph::NodeType::Subtraction:
		case InternalGraph::NodeType::Multiplication:
		case InternalGraph::NodeType::Division:
		case InternalGraph::NodeType::Power:
		case InternalGraph::NodeType::Max:
		case InternalGraph::NodeType::Min:
			AddNodeInputLinks(inputLinks,
				{ LinkType::Float, LinkType::Float });

			break;

		case InternalGraph::NodeType::Blend:
			AddNodeInputLinks(inputLinks,
				{ LinkType::Float, LinkType::Float, LinkType::Float, LinkType::Float, LinkType::Float });
			break;
		case InternalGraph::NodeType::Clamp:
			AddNodeInputLinks(inputLinks,
				{ LinkType::Float, LinkType::Float, LinkType::Float });
			break;
		case InternalGraph::NodeType::Selector:
			AddNodeInputLinks(inputLinks,
				{ LinkType::Float, LinkType::Float, LinkType::Float,  LinkType::Float,  LinkType::Float, LinkType::Float });
			break;

		case InternalGraph::NodeType::ConstantInt:
			outputType = LinkType::Int;
			inputLinks.push_back(InputLink(0));
			break;
		case InternalGraph::NodeType::ConstantFloat:
			inputLinks.push_back(InputLink(0.0f));
			break;
		case InternalGraph::NodeType::Invert:
			inputLinks.push_back(InputLink(0.0f));
			break;

		case InternalGraph::NodeType::TextureIndex:
			outputType = LinkType::Int;
			inputLinks.push_back(InputLink(0));
			break;

		case InternalGraph::NodeType::FractalReturnType:
			outputType = LinkType::Int;
			inputLinks.push_back(InputLink(0));
			break;
		case InternalGraph::NodeType::CellularReturnType:
			outputType = LinkType::Int;
			inputLinks.push_back(InputLink(0));
			break;

		case InternalGraph::NodeType::ValueNoise:
			AddNodeInputLinks(inputLinks,
				{ LinkType::Int, LinkType::Float, LinkType::Int, LinkType::Float, LinkType::Int });
			isNoiseNode = true;
			myNoise = FastNoiseSIMD::NewFastNoiseSIMD();
			break;

		case InternalGraph::NodeType::SimplexNoise:
			AddNodeInputLinks(inputLinks,
				{ LinkType::Int, LinkType::Float, LinkType::Int, LinkType::Float, LinkType::Int });
			isNoiseNode = true;
			myNoise = FastNoiseSIMD::NewFastNoiseSIMD();
			break;

		case InternalGraph::NodeType::PerlinNoise:
			AddNodeInputLinks(inputLinks,
				{ LinkType::Int, LinkType::Float, LinkType::Int, LinkType::Float, LinkType::Int });
			isNoiseNode = true;
			myNoise = FastNoiseSIMD::NewFastNoiseSIMD();
			break;

		case InternalGraph::NodeType::CubicNoise:
			AddNodeInputLinks(inputLinks,
				{ LinkType::Int, LinkType::Float, LinkType::Int, LinkType::Float, LinkType::Int });
			isNoiseNode = true;
			myNoise = FastNoiseSIMD::NewFastNoiseSIMD();
			break;

		case InternalGraph::NodeType::WhiteNoise:
			AddNodeInputLinks(inputLinks,
				{ LinkType::Int, LinkType::Float });
			isNoiseNode = true;
			myNoise = FastNoiseSIMD::NewFastNoiseSIMD();
			break;

		case InternalGraph::NodeType::CellNoise:
			AddNodeInputLinks(inputLinks,
				{ LinkType::Int, LinkType::Float, LinkType::Float, LinkType::Int });
			isNoiseNode = true;
			myNoise = FastNoiseSIMD::NewFastNoiseSIMD();
			break;

		case InternalGraph::NodeType::VoroniNoise:
			AddNodeInputLinks(inputLinks,
				{ LinkType::Int, LinkType::Float, LinkType::Float, LinkType::Int });
			isNoiseNode = true;
			myNoise = FastNoiseSIMD::NewFastNoiseSIMD();
			break;

		case NodeType::ColorCreator:
			outputType = LinkType::Vec4;
			AddNodeInputLinks(inputLinks,
				{ LinkType::Float, LinkType::Float, LinkType::Float, LinkType::Float });
			break;

		case NodeType::MonoGradient:
			AddNodeInputLinks(inputLinks,
				{ LinkType::Float, LinkType::Float, LinkType::Float, LinkType::Float, LinkType::Float });

			break;

		default:
			break;
		}


	}

	void Node::SetLinkValue(const int index, const LinkTypeVariants data) {
		inputLinks.at(index).SetDataValue(data);
	}

	LinkTypeVariants Node::GetHeightMapValue(const int x, const int z) const {
		return std::get<float>(inputLinks.at(0).GetValue(x, z)) * 2 - 1;
	}

	LinkTypeVariants Node::GetSplatMapValue(const int x, const int z) const {
		return inputLinks.at(1).GetValue(x, z);
	}

	LinkTypeVariants Node::GetValue(const int x, const int z) const {
		LinkTypeVariants retVal;
		//LinkTypeVariants reA, reB;
		float a, b, c, d, alpha;
		float value, lower, upper, smooth;

		auto val = (inputLinks.at(0));


		switch (nodeType)
		{
		case InternalGraph::NodeType::None:
			break;

		case InternalGraph::NodeType::Addition:

			switch (outputType)
			{
			case InternalGraph::LinkType::None:
				break;
			case InternalGraph::LinkType::Float:
				return std::get<float>(inputLinks.at(0).GetValue(x, z)) + std::get<float>(inputLinks.at(1).GetValue(x, z));
				break;
			case InternalGraph::LinkType::Int:
				return std::get<int>(inputLinks.at(0).GetValue(x, z)) + std::get<int>(inputLinks.at(1).GetValue(x, z));
				break;
			case InternalGraph::LinkType::Vec2:
				return std::get<glm::vec2>(inputLinks.at(0).GetValue(x, z)) + std::get<glm::vec2>(inputLinks.at(1).GetValue(x, z));
				break;
			case InternalGraph::LinkType::Vec3:
				return std::get<glm::vec3>(inputLinks.at(0).GetValue(x, z)) + std::get<glm::vec3>(inputLinks.at(1).GetValue(x, z));
				break;
			case InternalGraph::LinkType::Vec4:
				return std::get<glm::vec4>(inputLinks.at(0).GetValue(x, z)) + std::get<glm::vec4>(inputLinks.at(1).GetValue(x, z));
				break;
			default:
				break;
			}
			break;

		case InternalGraph::NodeType::Subtraction:
			switch (outputType)
			{
			case InternalGraph::LinkType::None:
				break;
			case InternalGraph::LinkType::Float:
				return std::get<float>(inputLinks.at(0).GetValue(x, z)) - std::get<float>(inputLinks.at(1).GetValue(x, z));
				break;
			case InternalGraph::LinkType::Int:
				return std::get<int>(inputLinks.at(0).GetValue(x, z)) - std::get<int>(inputLinks.at(1).GetValue(x, z));
				break;
			case InternalGraph::LinkType::Vec2:
				return std::get<glm::vec2>(inputLinks.at(0).GetValue(x, z)) - std::get<glm::vec2>(inputLinks.at(1).GetValue(x, z));
				break;
			case InternalGraph::LinkType::Vec3:
				return std::get<glm::vec3>(inputLinks.at(0).GetValue(x, z)) - std::get<glm::vec3>(inputLinks.at(1).GetValue(x, z));
				break;
			case InternalGraph::LinkType::Vec4:
				return std::get<glm::vec4>(inputLinks.at(0).GetValue(x, z)) - std::get<glm::vec4>(inputLinks.at(1).GetValue(x, z));
				break;
			default:
				break;
			}
			break;
		case InternalGraph::NodeType::Multiplication:
			switch (outputType)
			{
			case InternalGraph::LinkType::None:
				break;
			case InternalGraph::LinkType::Float:
				return std::get<float>(inputLinks.at(0).GetValue(x, z)) * std::get<float>(inputLinks.at(1).GetValue(x, z));
				break;
			case InternalGraph::LinkType::Int:
				return std::get<int>(inputLinks.at(0).GetValue(x, z)) * std::get<int>(inputLinks.at(1).GetValue(x, z));
				break;
			case InternalGraph::LinkType::Vec2:
				return std::get<glm::vec2>(inputLinks.at(0).GetValue(x, z)) * std::get<glm::vec2>(inputLinks.at(1).GetValue(x, z));
				break;
			case InternalGraph::LinkType::Vec3:
				return std::get<glm::vec3>(inputLinks.at(0).GetValue(x, z)) * std::get<glm::vec3>(inputLinks.at(1).GetValue(x, z));
				break;
			case InternalGraph::LinkType::Vec4:
				return std::get<glm::vec4>(inputLinks.at(0).GetValue(x, z)) * std::get<glm::vec4>(inputLinks.at(1).GetValue(x, z));
				break;
			default:
				break;
			}
			break;
		case InternalGraph::NodeType::Division:
			switch (outputType)
			{
			case InternalGraph::LinkType::None:
				break;
			case InternalGraph::LinkType::Float:
				return std::get<float>(inputLinks.at(0).GetValue(x, z)) / std::get<float>(inputLinks.at(1).GetValue(x, z));
				break;
			case InternalGraph::LinkType::Int:
				return std::get<int>(inputLinks.at(0).GetValue(x, z)) / std::get<int>(inputLinks.at(1).GetValue(x, z));
				break;
			case InternalGraph::LinkType::Vec2:
				return std::get<glm::vec2>(inputLinks.at(0).GetValue(x, z)) / std::get<glm::vec2>(inputLinks.at(1).GetValue(x, z));
				break;
			case InternalGraph::LinkType::Vec3:
				return std::get<glm::vec3>(inputLinks.at(0).GetValue(x, z)) / std::get<glm::vec3>(inputLinks.at(1).GetValue(x, z));
				break;
			case InternalGraph::LinkType::Vec4:
				return std::get<glm::vec4>(inputLinks.at(0).GetValue(x, z)) / std::get<glm::vec4>(inputLinks.at(1).GetValue(x, z));
				break;
			default:
				break;
			}
			break;
		case InternalGraph::NodeType::Power:
			switch (outputType)
			{
			case InternalGraph::LinkType::None:
				break;
			case InternalGraph::LinkType::Float:
				return glm::pow(std::get<float>(inputLinks.at(0).GetValue(x, z)), std::get<float>(inputLinks.at(1).GetValue(x, z)));
				break;
			case InternalGraph::LinkType::Int:
				return glm::pow((float)std::get<int>(inputLinks.at(0).GetValue(x, z)), (float)std::get<int>(inputLinks.at(1).GetValue(x, z)));
				break;
			case InternalGraph::LinkType::Vec2:
				return glm::pow(std::get<glm::vec2>(inputLinks.at(0).GetValue(x, z)), std::get<glm::vec2>(inputLinks.at(1).GetValue(x, z)));
				break;
			case InternalGraph::LinkType::Vec3:
				return glm::pow(std::get<glm::vec3>(inputLinks.at(0).GetValue(x, z)), std::get<glm::vec3>(inputLinks.at(1).GetValue(x, z)));
				break;
			case InternalGraph::LinkType::Vec4:
				return glm::pow(std::get<glm::vec4>(inputLinks.at(0).GetValue(x, z)), std::get<glm::vec4>(inputLinks.at(1).GetValue(x, z)));
				break;
			default:
				break;
			}
			break;
		case InternalGraph::NodeType::Max:
			switch (outputType)
			{
			case InternalGraph::LinkType::None:
				break;
			case InternalGraph::LinkType::Float:
				return glm::max(std::get<float>(inputLinks.at(0).GetValue(x, z)), std::get<float>(inputLinks.at(1).GetValue(x, z)));
				break;
			case InternalGraph::LinkType::Int:
				return glm::max(std::get<int>(inputLinks.at(0).GetValue(x, z)), std::get<int>(inputLinks.at(1).GetValue(x, z)));
				break;
			case InternalGraph::LinkType::Vec2:
				return glm::max(std::get<glm::vec2>(inputLinks.at(0).GetValue(x, z)), std::get<glm::vec2>(inputLinks.at(1).GetValue(x, z)));
				break;
			case InternalGraph::LinkType::Vec3:
				return glm::max(std::get<glm::vec3>(inputLinks.at(0).GetValue(x, z)), std::get<glm::vec3>(inputLinks.at(1).GetValue(x, z)));
				break;
			case InternalGraph::LinkType::Vec4:
				return glm::max(std::get<glm::vec4>(inputLinks.at(0).GetValue(x, z)), std::get<glm::vec4>(inputLinks.at(1).GetValue(x, z)));
				break;
			default:
				break;
			}
			break;

		case InternalGraph::NodeType::Min:
			switch (outputType)
			{
			case InternalGraph::LinkType::None:
				break;
			case InternalGraph::LinkType::Float:
				return glm::min(std::get<float>(inputLinks.at(0).GetValue(x, z)), std::get<float>(inputLinks.at(1).GetValue(x, z)));
				break;
			case InternalGraph::LinkType::Int:
				return glm::min(std::get<int>(inputLinks.at(0).GetValue(x, z)), std::get<int>(inputLinks.at(1).GetValue(x, z)));
				break;
			case InternalGraph::LinkType::Vec2:
				return glm::min(std::get<glm::vec2>(inputLinks.at(0).GetValue(x, z)), std::get<glm::vec2>(inputLinks.at(1).GetValue(x, z)));
				break;
			case InternalGraph::LinkType::Vec3:
				return glm::min(std::get<glm::vec3>(inputLinks.at(0).GetValue(x, z)), std::get<glm::vec3>(inputLinks.at(1).GetValue(x, z)));
				break;
			case InternalGraph::LinkType::Vec4:
				return glm::min(std::get<glm::vec4>(inputLinks.at(0).GetValue(x, z)), std::get<glm::vec4>(inputLinks.at(1).GetValue(x, z)));
				break;
			default:
				break;
			}
			break;
		case InternalGraph::NodeType::Blend:
			switch (outputType)
			{
			case InternalGraph::LinkType::Float:
				a = std::get<float>(inputLinks.at(0).GetValue(x, z));
				b = std::get<float>(inputLinks.at(1).GetValue(x, z));
				alpha = std::get<float>(inputLinks.at(2).GetValue(x, z));
				retVal = alpha * b + (1 - alpha) * a;
				return retVal;
			default:
				break;
			}
			break;
		case InternalGraph::NodeType::Clamp:
			switch (outputType)
			{
			case InternalGraph::LinkType::Float:
				value = std::get<float>(inputLinks.at(0).GetValue(x, z));
				lower = std::get<float>(inputLinks.at(1).GetValue(x, z));
				upper = std::get<float>(inputLinks.at(2).GetValue(x, z));
				retVal = glm::clamp(value, lower, upper);
				return retVal;
			default:
				break;
			}
			break;

		case InternalGraph::NodeType::Selector:
			value = std::get<float>(inputLinks.at(0).GetValue(x, z));
			a = std::get<float>(inputLinks.at(1).GetValue(x, z));
			b = std::get<float>(inputLinks.at(2).GetValue(x, z));
			lower = std::get<float>(inputLinks.at(3).GetValue(x, z));
			upper = std::get<float>(inputLinks.at(4).GetValue(x, z));
			smooth = std::get<float>(inputLinks.at(5).GetValue(x, z));

			if (smooth == 0) {
				if (value < lower && value > upper)
					return a;
				else
					return b;
			}
			if (value < lower - smooth / 2.0f) {
				return a;
			}
			else if (value >= lower - smooth / 2.0f && value < lower + smooth / 2.0f) {
				return ((value - (lower - smooth / 2.0f)) / smooth) * b
					+ (1 - ((value - (lower - smooth / 2.0f)) / smooth)) * a;
			}
			else if (value >= lower + smooth / 2.0f && value <= upper - smooth / 2.0f) {
				return b;

			}
			else if (value > upper - smooth / 2.0f  && value <= upper + smooth / 2.0f) {
				return (((upper + smooth / 2.0f) - value) / smooth) * b
					+ (1 - (((upper + smooth / 2.0f) - value) / smooth)) * a;

			}
			else if (value > upper + smooth / 2.0f)
				return a;
			break;
		case InternalGraph::NodeType::ConstantInt:
			return inputLinks.at(0).GetValue(x, z);

			break;
		case InternalGraph::NodeType::ConstantFloat:
			return inputLinks.at(0).GetValue(x, z);

			break;

		case InternalGraph::NodeType::Invert:
			value = std::get<float>(inputLinks.at(0).GetValue(x, z));
			retVal = 1 - value;
			return retVal;

			break;

		case InternalGraph::NodeType::TextureIndex:
			return inputLinks.at(0).GetValue(x, z);

			break;

		case InternalGraph::NodeType::FractalReturnType:
			return inputLinks.at(0).GetValue(x, z);

		case InternalGraph::NodeType::CellularReturnType:
			return inputLinks.at(0).GetValue(x, z);

			break;

		case InternalGraph::NodeType::ValueNoise:
		case InternalGraph::NodeType::SimplexNoise:
		case InternalGraph::NodeType::PerlinNoise:
		case InternalGraph::NodeType::WhiteNoise:
		case InternalGraph::NodeType::CellNoise:
		case InternalGraph::NodeType::CubicNoise:
		case InternalGraph::NodeType::VoroniNoise:

			retVal = (noiseImage.BoundedLookUp(x, z) + 1.0f) / 2.0f;
			return retVal;
			//Log::Debug << val << "\n";

			break;

		case NodeType::ColorCreator:

			a = std::get<float>(inputLinks.at(0).GetValue(x, z));
			b = std::get<float>(inputLinks.at(1).GetValue(x, z));
			c = std::get<float>(inputLinks.at(2).GetValue(x, z));
			d = std::get<float>(inputLinks.at(3).GetValue(x, z));

			retVal = glm::vec4(a, b, c, d);

			return retVal;
			break;

		case NodeType::MonoGradient:
			value = std::get<float>(inputLinks.at(0).GetValue(x, z));
			lower = std::get<float>(inputLinks.at(1).GetValue(x, z));
			upper = std::get<float>(inputLinks.at(2).GetValue(x, z));
			smooth = std::get<float>(inputLinks.at(3).GetValue(x, z));
			//value = glm::clamp(value, lower, upper);
			retVal = lower + value * (upper - lower);
			//retVal = ((upper - value) + (value - lower)) / (upper - lower);

			return retVal;

			break;

		default:
			break;
		}
		return retVal;
	}

	NodeType Node::GetNodeType() const {
		return nodeType;
	}

	LinkType Node::GetOutputType() const {
		return outputType;
	}


	void Node::SetLinkInput(const int index, const NodeID id) {
		inputLinks.at(index).SetInputNode(id);
	}

	void Node::ResetLinkInput(const int index) {
		inputLinks.at(index).ResetInputNode();
	}

	void Node::SetID(NodeID id) {
		this->id = id;
	}
	NodeID Node::GetID() {
		return id;
	}

	void Node::SetIsNoiseNode(bool val) {
		isNoiseNode = val;
	}

	void Node::SetFractalType(int val) {
		if (val == 2)
			myNoise->SetFractalType(FastNoiseSIMD::FractalType::RigidMulti);
		else if (val == 1)
			myNoise->SetFractalType(FastNoiseSIMD::FractalType::Billow);
		else
			myNoise->SetFractalType(FastNoiseSIMD::FractalType::FBM);

	}

	void Node::SetCellularDistanceFunction(int index) {
		if (index == 2)
			myNoise->SetCellularDistanceFunction(FastNoiseSIMD::CellularDistanceFunction::Natural);
		else if (index == 1)
			myNoise->SetCellularDistanceFunction(FastNoiseSIMD::CellularDistanceFunction::Manhattan);
		else
			myNoise->SetCellularDistanceFunction(FastNoiseSIMD::CellularDistanceFunction::Euclidean);
	}
	void Node::SetCellularReturnType(int index) {
		if (index == 1)
			myNoise->SetCellularReturnType(FastNoiseSIMD::CellularReturnType::Distance);
		else if (index == 2)
			myNoise->SetCellularReturnType(FastNoiseSIMD::CellularReturnType::Distance2);
		else if (index == 3)
			myNoise->SetCellularReturnType(FastNoiseSIMD::CellularReturnType::Distance2Add);
		else if (index == 4)
			myNoise->SetCellularReturnType(FastNoiseSIMD::CellularReturnType::Distance2Sub);
		else if (index == 5)
			myNoise->SetCellularReturnType(FastNoiseSIMD::CellularReturnType::Distance2Mul);
		else if (index == 6)
			myNoise->SetCellularReturnType(FastNoiseSIMD::CellularReturnType::Distance2Div);
		else if (index == 7)
			myNoise->SetCellularReturnType(FastNoiseSIMD::CellularReturnType::Distance2Cave);
		else
			myNoise->SetCellularReturnType(FastNoiseSIMD::CellularReturnType::CellValue);
	}

	void Node::SetupInputLinks(NodeMap* map) {
		for (auto& link : inputLinks) {
			if (link.HasInputNode()) {
				Node* ptr = &(map->at(link.GetInputNode()));
				link.SetInputNodePointer(ptr);
			}
		}
	}

	void Node::SetupNodeForComputation(NoiseSourceInfo info) {
		if (isNoiseNode) {
			noiseImage.SetWidth(info.cellsWide);

			myNoise->SetSeed(std::get<int>(inputLinks.at(0).GetValue()));


			myNoise->SetFrequency(std::get<float>(inputLinks.at(1).GetValue()));

			//myNoise->SetAxisScales(info.scale, info.scale, info.scale);
			switch (nodeType)
			{
			case InternalGraph::NodeType::WhiteNoise:
				noiseImage.SetImageData(info.cellsWide, myNoise->GetWhiteNoiseSet(info.pos.x, 0, info.pos.y, info.cellsWide, 1, info.cellsWide, info.scale));
				break;

			case InternalGraph::NodeType::ValueNoise:
				myNoise->SetFractalOctaves(std::get<int>(inputLinks.at(2).GetValue()));
				myNoise->SetFractalGain(std::get<float>(inputLinks.at(3).GetValue()));
				SetFractalType(std::get<int>(inputLinks.at(4).GetValue()));
				noiseImage.SetImageData(info.cellsWide, myNoise->GetValueFractalSet(info.pos.x, 0, info.pos.y, info.cellsWide, 1, info.cellsWide, info.scale));
				break;

			case InternalGraph::NodeType::SimplexNoise:
				myNoise->SetFractalOctaves(std::get<int>(inputLinks.at(2).GetValue()));
				myNoise->SetFractalGain(std::get<float>(inputLinks.at(3).GetValue()));
				SetFractalType(std::get<int>(inputLinks.at(4).GetValue()));
				noiseImage.SetImageData(info.cellsWide, myNoise->GetSimplexFractalSet(info.pos.x, 0, info.pos.y, info.cellsWide, 1, info.cellsWide, info.scale));
				break;

			case InternalGraph::NodeType::PerlinNoise:
				myNoise->SetFractalOctaves(std::get<int>(inputLinks.at(2).GetValue()));
				myNoise->SetFractalGain(std::get<float>(inputLinks.at(3).GetValue()));
				SetFractalType(std::get<int>(inputLinks.at(4).GetValue()));
				noiseImage.SetImageData(info.cellsWide, myNoise->GetPerlinFractalSet(info.pos.x, 0, info.pos.y, info.cellsWide, 1, info.cellsWide, info.scale));
				break;

			case InternalGraph::NodeType::CubicNoise:
				myNoise->SetFractalOctaves(std::get<int>(inputLinks.at(2).GetValue()));
				myNoise->SetFractalGain(std::get<float>(inputLinks.at(3).GetValue()));
				SetFractalType(std::get<int>(inputLinks.at(4).GetValue()));
				noiseImage.SetImageData(info.cellsWide, myNoise->GetCubicFractalSet(info.pos.x, 0, info.pos.y, info.cellsWide, 1, info.cellsWide, info.scale));
				break;

			case InternalGraph::NodeType::CellNoise:
				myNoise->SetCellularJitter(std::get<float>(inputLinks.at(2).GetValue()));
				SetCellularReturnType(std::get<int>(inputLinks.at(3).GetValue()));
				noiseImage.SetImageData(info.cellsWide, myNoise->GetCellularSet(info.pos.x, 0, info.pos.y, info.cellsWide, 1, info.cellsWide, info.scale));
				break;

			case InternalGraph::NodeType::VoroniNoise:
				myNoise->SetCellularJitter(std::get<float>(inputLinks.at(2).GetValue()));
				myNoise->SetCellularReturnType(FastNoiseSIMD::CellularReturnType::CellValue);
				noiseImage.SetImageData(info.cellsWide, myNoise->GetCellularSet(info.pos.x, 0, info.pos.y, info.cellsWide, 1, info.cellsWide, info.scale));
				break;

			default:
				break;
			}


		}
	}


	void Node::CleanNoise() {
		if (isNoiseNode)
			myNoise->FreeNoiseSet(noiseImage.GetImageData());
	}

	GraphPrototype::GraphPrototype() {
		//Node outputNode(NodeType::Output);
		//outputNodeID = AddNode(outputNode);

	}
	GraphPrototype::~GraphPrototype() {
	}

	void GraphPrototype::ResetGraph() {
		nodeMap.clear();
		nodeIDCounter = 0;
		outputNodeID = 0;
	}

	NodeID GraphPrototype::AddNode(Node node) {
		node.SetID(GetNextID());
		nodeMap[node.GetID()] = node;
		if (node.GetNodeType() == NodeType::Output) {
			outputNodeID = node.GetID();
		}
		return node.GetID();
	}
	NodeID GraphPrototype::AddNoiseNoide(Node node) {
		node.SetID(GetNextID());
		node.SetIsNoiseNode(true);
		nodeMap[node.GetID()] = node;

		return node.GetID();
	}

	bool GraphPrototype::DeleteNode(NodeID id) {
		auto val = nodeMap.find(id);
		if (val != nodeMap.end()) {
			nodeMap.erase(id);
			return true;
		}
		else return false;
	}

	Node& GraphPrototype::GetNodeByID(NodeID id) {
		return nodeMap.at(id);

	}
	NodeID GraphPrototype::GetNextID() {
		return nodeIDCounter++;
	}

	NodeID GraphPrototype::GetOutputNodeID() const {
		return outputNodeID;
	}

	void GraphPrototype::SetOutputNodeID(NodeID id) {
		outputNodeID = id;
	}

	NodeMap GraphPrototype::GetNodeMap() const {
		return nodeMap;
	}


	GraphUser::GraphUser(const GraphPrototype& graph,
		int seed, int cellsWide, glm::i32vec2 pos, float scale) :
		info(seed, cellsWide, scale, pos)
	{
		//glm::ivec2(pos.x * (cellsWide) / scale, pos.y * (cellsWide) / scale), scale / (cellsWide)

		//NoiseSourceInfo info = NoiseSourceInfo(seed, cellsWide, scale, pos);
		this->nodeMap = graph.GetNodeMap();

		//auto&!!!
		for (auto& node : nodeMap)// it = nodeMap.begin(); it != nodeMap.end(); it++)
		{
			for (auto& link : node.second.inputLinks) {// linkIt = node.second.inputLinks.begin(); linkIt != it->second.inputLinks.end(); linkIt++) {
				if (link.HasInputNode()) {
					Node* n = &nodeMap.at(link.GetInputNode());
					link.SetInputNodePointer(n);
				}
			}
		}

		for (auto& node : nodeMap) {
			node.second.SetupNodeForComputation(info);
		}

		outputNode = &nodeMap[graph.GetOutputNodeID()];

		outputHeightMap = NoiseImage2D<float>(cellsWide);
		for (int x = 0; x < cellsWide; x++)
		{
			for (int z = 0; z < cellsWide; z++)
			{
				float val = std::get<float>(outputNode->GetHeightMapValue(x, z));
				outputHeightMap.SetPixelValue(x, z, val);
			}
		}

		outputSplatmap = std::vector<std::byte>(cellsWide * cellsWide * 4);
		int i = 0;
		for (int x = 0; x < cellsWide; x++)
		{
			for (int z = 0; z < cellsWide; z++)
			{
				glm::vec4 val = glm::normalize(std::get<glm::vec4>(outputNode->GetSplatMapValue(z, x)));
				//Resource::Texture::Pixel_RGBA pixel = Resource::Texture::Pixel_RGBA(

				std::byte r = static_cast<std::byte>(static_cast<uint8_t>(glm::clamp(val.x, 0.0f, 1.0f) * 255.0f));
				std::byte g = static_cast<std::byte>(static_cast<uint8_t>(glm::clamp(val.y, 0.0f, 1.0f) * 255.0f));
				std::byte b = static_cast<std::byte>(static_cast<uint8_t>(glm::clamp(val.z, 0.0f, 1.0f) * 255.0f));
				std::byte a = static_cast<std::byte>(static_cast<uint8_t>(glm::clamp(val.w, 0.0f, 1.0f) * 255.0f));

				outputSplatmap.at(i++) = r;
				outputSplatmap.at(i++) = g;
				outputSplatmap.at(i++) = b;
				outputSplatmap.at(i++) = a;
			}
		}
		glm::vec4 val = glm::normalize(std::get<glm::vec4>(outputNode->GetSplatMapValue(0, 0)));

		//Log::Debug << val.x << " "<< val.y << " "<< val.z << " "<< val.w << " " << "\n";



		for (auto&[key, val] : nodeMap) {
			val.CleanNoise();
		}
	}

	const float GraphUser::SampleHeightMap(const float x, const float z) const {
		return BilinearImageSample2D(outputHeightMap, x, z);
	}
	NoiseImage2D<float>& GraphUser::GetHeightMap() {
		return outputHeightMap;

	}

	std::byte* GraphUser::GetSplatMapPtr() {
		return outputSplatmap.data();

	}


	NoiseImage2D<uint8_t>& GraphUser::GetVegetationDensityMap() {
		return vegetationDensityMap;

	}

}