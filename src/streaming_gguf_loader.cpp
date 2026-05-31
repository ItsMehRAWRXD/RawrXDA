#include "streaming_gguf_loader.h"

#include <algorithm>
#include <cstring>
#include <limits>

namespace RawrXD {

StreamingGGUFLoader::StreamingGGUFLoader()
	: is_open_(false), using_mmap_(false), tensor_info_offset_(0), current_zone_memory_(0),
	  max_zone_memory_mb_(512), data_section_offset_(0) {
	std::memset(&header_, 0, sizeof(GGUFHeader));
}

StreamingGGUFLoader::~StreamingGGUFLoader() {
	Close();
}

bool StreamingGGUFLoader::Open(const std::string& filepath) {
	Close();
	filepath_ = filepath;
	file_.open(filepath, std::ios::binary);
	if (!file_.is_open()) {
		return false;
	}

	is_open_ = true;
	if (!ParseHeader() || !ParseMetadata() || !BuildTensorIndex()) {
		Close();
		return false;
	}

	AssignTensorsToZones();
	return true;
}

bool StreamingGGUFLoader::Close() {
	if (file_.is_open()) {
		file_.close();
	}

	is_open_ = false;
	using_mmap_ = false;
	mmap_streamer_.reset();
	filepath_.clear();
	tensor_info_offset_ = 0;
	data_section_offset_ = 0;
	tensor_index_.clear();
	tensor_enumeration_order_.clear();
	zones_.clear();
	active_zones_.clear();
	tensor_generation_ids_.clear();
	dirty_tensors_.clear();
	current_zone_.clear();
	current_zone_memory_ = 0;
	metadata_.kv_pairs.clear();
	m_vocab.clear();
	return true;
}

bool StreamingGGUFLoader::ParseHeader() {
	if (!file_.is_open()) {
		return false;
	}

	file_.clear();
	file_.seekg(0, std::ios::beg);

	if (!ReadValue(header_.magic)) {
		return false;
	}
	if (!ReadValue(header_.version)) {
		return false;
	}
	if (!ReadValue(header_.tensor_count)) {
		return false;
	}
	if (!ReadValue(header_.metadata_kv_count)) {
		return false;
	}

	header_.metadata_offset = static_cast<uint64_t>(file_.tellg());
	return true;
}

GGUFHeader StreamingGGUFLoader::GetHeader() const {
	return header_;
}

bool StreamingGGUFLoader::ParseMetadata() {
	if (!file_.is_open()) {
		return false;
	}

	metadata_.kv_pairs.clear();
	m_vocab.clear();

	file_.clear();
	file_.seekg(static_cast<std::streamoff>(header_.metadata_offset), std::ios::beg);

	for (uint64_t i = 0; i < header_.metadata_kv_count; ++i) {
		std::string key;
		if (!ReadString(key)) {
			return false;
		}

		uint32_t valueType = 0;
		if (!ReadValue(valueType)) {
			return false;
		}

		switch (valueType) {
		case 0: {
			uint8_t value = 0;
			if (!ReadValue(value)) return false;
			metadata_.kv_pairs[key] = std::to_string(value);
			break;
		}
		case 1: {
			int8_t value = 0;
			if (!ReadValue(value)) return false;
			metadata_.kv_pairs[key] = std::to_string(value);
			break;
		}
		case 2: {
			uint16_t value = 0;
			if (!ReadValue(value)) return false;
			metadata_.kv_pairs[key] = std::to_string(value);
			break;
		}
		case 3: {
			int16_t value = 0;
			if (!ReadValue(value)) return false;
			metadata_.kv_pairs[key] = std::to_string(value);
			break;
		}
		case 4: {
			uint32_t value = 0;
			if (!ReadValue(value)) return false;
			metadata_.kv_pairs[key] = std::to_string(value);
			break;
		}
		case 5: {
			int32_t value = 0;
			if (!ReadValue(value)) return false;
			metadata_.kv_pairs[key] = std::to_string(value);
			break;
		}
		case 6: {
			float value = 0.0f;
			if (!ReadValue(value)) return false;
			metadata_.kv_pairs[key] = std::to_string(value);
			break;
		}
		case 7: {
			uint8_t value = 0;
			if (!ReadValue(value)) return false;
			metadata_.kv_pairs[key] = value ? "true" : "false";
			break;
		}
		case 8: {
			std::string value;
			if (!ReadString(value)) return false;
			metadata_.kv_pairs[key] = value;
			break;
		}
		case 9: {
			uint32_t arrayType = 0;
			uint64_t arrayLength = 0;
			if (!ReadValue(arrayType) || !ReadValue(arrayLength)) {
				return false;
			}

			if (key == "tokenizer.ggml.tokens" && arrayType == 8) {
				m_vocab.reserve(static_cast<size_t>(arrayLength));
				for (uint64_t idx = 0; idx < arrayLength; ++idx) {
					std::string token;
					if (!ReadString(token)) return false;
					m_vocab.push_back(token);
				}
				metadata_.kv_pairs[key] = "<array>";
			} else {
				for (uint64_t idx = 0; idx < arrayLength; ++idx) {
					switch (arrayType) {
					case 0: { uint8_t v = 0; if (!ReadValue(v)) return false; break; }
					case 1: { int8_t v = 0; if (!ReadValue(v)) return false; break; }
					case 2: { uint16_t v = 0; if (!ReadValue(v)) return false; break; }
					case 3: { int16_t v = 0; if (!ReadValue(v)) return false; break; }
					case 4: { uint32_t v = 0; if (!ReadValue(v)) return false; break; }
					case 5: { int32_t v = 0; if (!ReadValue(v)) return false; break; }
					case 6: { float v = 0.0f; if (!ReadValue(v)) return false; break; }
					case 7: { uint8_t v = 0; if (!ReadValue(v)) return false; break; }
					case 8: { std::string v; if (!ReadString(v)) return false; break; }
					case 10: { uint64_t v = 0; if (!ReadValue(v)) return false; break; }
					case 11: { int64_t v = 0; if (!ReadValue(v)) return false; break; }
					case 12: { double v = 0.0; if (!ReadValue(v)) return false; break; }
					default:
						return false;
					}
				}
				metadata_.kv_pairs[key] = "<array>";
			}
			break;
		}
		case 10: {
			uint64_t value = 0;
			if (!ReadValue(value)) return false;
			metadata_.kv_pairs[key] = std::to_string(value);
			break;
		}
		case 11: {
			int64_t value = 0;
			if (!ReadValue(value)) return false;
			metadata_.kv_pairs[key] = std::to_string(value);
			break;
		}
		case 12: {
			double value = 0.0;
			if (!ReadValue(value)) return false;
			metadata_.kv_pairs[key] = std::to_string(value);
			break;
		}
		default:
			return false;
		}
	}

	tensor_info_offset_ = static_cast<uint64_t>(file_.tellg());

	auto findUint = [this](const char* key) -> uint64_t {
		const auto it = metadata_.kv_pairs.find(key);
		if (it == metadata_.kv_pairs.end()) {
			return 0;
		}
		try {
			return std::stoull(it->second);
		} catch (...) {
			return 0;
		}
	};

	metadata_.architecture_type = metadata_.kv_pairs.count("general.architecture") ? 1u : 0u;
	metadata_.layer_count = static_cast<uint32_t>(findUint("llama.block_count"));
	metadata_.context_length = static_cast<uint32_t>(findUint("llama.context_length"));
	metadata_.embedding_dim = static_cast<uint32_t>(findUint("llama.embedding_length"));
	metadata_.vocab_size = static_cast<uint32_t>(findUint("llama.vocab_size"));

	return true;
}

GGUFMetadata StreamingGGUFLoader::GetMetadata() const {
	return metadata_;
}

bool StreamingGGUFLoader::BuildTensorIndex() {
	if (!file_.is_open() || tensor_info_offset_ == 0) {
		return false;
	}

	tensor_index_.clear();
	tensor_enumeration_order_.clear();

	file_.clear();
	file_.seekg(static_cast<std::streamoff>(tensor_info_offset_), std::ios::beg);

	for (uint64_t index = 0; index < header_.tensor_count; ++index) {
		TensorRef ref;
		if (!ReadString(ref.name)) {
			return false;
		}

		uint32_t dims = 0;
		if (!ReadValue(dims)) {
			return false;
		}

		ref.shape.resize(dims);
		for (uint32_t dim = 0; dim < dims; ++dim) {
			if (!ReadValue(ref.shape[dim])) {
				return false;
			}
		}

		uint32_t typeValue = 0;
		if (!ReadValue(typeValue)) {
			return false;
		}
		ref.type = static_cast<GGMLType>(typeValue);

		if (!ReadValue(ref.offset)) {
			return false;
		}

		ref.index = index;
		ref.size = CalculateTensorSize(ref.shape, ref.type);
		ref.zone_name.clear();

		tensor_index_[ref.name] = ref;
		tensor_enumeration_order_.push_back(ref);
	}

	uint64_t alignment = 32;
	const auto alignmentIt = metadata_.kv_pairs.find("general.alignment");
	if (alignmentIt != metadata_.kv_pairs.end()) {
		try {
			alignment = std::max<uint64_t>(1, std::stoull(alignmentIt->second));
		} catch (...) {
			alignment = 32;
		}
	}

	const uint64_t currentPos = static_cast<uint64_t>(file_.tellg());
	const uint64_t remainder = currentPos % alignment;
	data_section_offset_ = remainder == 0 ? currentPos : currentPos + (alignment - remainder);
	return true;
}

bool StreamingGGUFLoader::LoadZone(const std::string& zone_name, uint64_t max_memory_mb) {
	max_zone_memory_mb_ = max_memory_mb;
	auto zoneIt = zones_.find(zone_name);
	if (zoneIt == zones_.end()) {
		return false;
	}

	if (zoneIt->second.is_loaded) {
		return true;
	}

	if (!current_zone_.empty() && current_zone_ != zone_name) {
		UnloadZone(current_zone_);
	}

	if (!StreamZoneFromDisk(zone_name)) {
		return LoadZoneMapped(zone_name);
	}

	current_zone_ = zone_name;
	return true;
}

bool StreamingGGUFLoader::UnloadZone(const std::string& zone_name) {
	const auto zoneIt = zones_.find(zone_name);
	if (zoneIt == zones_.end()) {
		return false;
	}

	TensorZoneInfo& zone = zoneIt->second;
	zone.data.clear();
	zone.data.shrink_to_fit();
	zone.is_loaded = false;
	if (current_zone_ == zone_name) {
		current_zone_.clear();
		current_zone_memory_ = 0;
	}
	return true;
}

std::vector<std::string> StreamingGGUFLoader::GetLoadedZones() const {
	std::vector<std::string> loaded;
	for (const auto& [zoneName, zoneInfo] : zones_) {
		if (zoneInfo.is_loaded) {
			loaded.push_back(zoneName);
		}
	}
	return loaded;
}

std::vector<std::string> StreamingGGUFLoader::GetAllZones() const {
	std::vector<std::string> names;
	names.reserve(zones_.size());
	for (const auto& [zoneName, _] : zones_) {
		names.push_back(zoneName);
	}
	return names;
}

std::vector<TensorInfo> StreamingGGUFLoader::GetAllTensorInfo() const {
	std::vector<TensorInfo> result;
	result.reserve(tensor_enumeration_order_.size());
	for (const auto& ref : tensor_enumeration_order_) {
		TensorInfo info;
		info.name = ref.name;
		info.shape = ref.shape;
		info.type = ref.type;
		info.offset = ref.offset;
		info.size = ref.size;
		info.size_bytes = ref.size;
		result.push_back(info);
	}
	return result;
}

std::vector<TensorInfo> StreamingGGUFLoader::GetTensorInfo() const {
	return GetAllTensorInfo();
}

uint64_t StreamingGGUFLoader::GetCurrentMemoryUsage() const {
	uint64_t total = 0;
	for (const auto& [_, zoneInfo] : zones_) {
		if (zoneInfo.is_loaded) {
			total += static_cast<uint64_t>(zoneInfo.data.size());
		}
	}
	return total;
}

TensorZoneInfo StreamingGGUFLoader::GetZoneInfo(const std::string& zone_name) const {
	const auto it = zones_.find(zone_name);
	return it != zones_.end() ? it->second : TensorZoneInfo{};
}

std::vector<TensorRef> StreamingGGUFLoader::GetTensorIndex() const {
	return tensor_enumeration_order_;
}

std::string StreamingGGUFLoader::GetTensorZone(const std::string& tensor_name) const {
	return GetZoneForTensor(tensor_name);
}

bool StreamingGGUFLoader::GetTensorData(const std::string& tensor_name, std::vector<uint8_t>& data) {
	const auto tensorIt = tensor_index_.find(tensor_name);
	if (tensorIt == tensor_index_.end()) {
		return false;
	}

	const std::string zoneName = tensorIt->second.zone_name;
	if (zoneName.empty()) {
		return false;
	}

	if (!zones_[zoneName].is_loaded && !LoadZone(zoneName, max_zone_memory_mb_)) {
		return false;
	}

	const TensorZoneInfo& zone = zones_[zoneName];
	uint64_t offsetInZone = 0;
	for (const auto& other : zone.tensors) {
		const auto& ref = tensor_index_.at(other);
		if (other == tensor_name) {
			if (offsetInZone + ref.size > zone.data.size()) {
				return false;
			}
			data.assign(zone.data.begin() + static_cast<std::ptrdiff_t>(offsetInZone),
						zone.data.begin() + static_cast<std::ptrdiff_t>(offsetInZone + ref.size));
			return true;
		}
		offsetInZone += ref.size;
	}

	return false;
}

bool StreamingGGUFLoader::ProbeTensorData(const std::string& tensor_name, size_t sample_bytes) {
	const auto it = tensor_index_.find(tensor_name);
	if (it == tensor_index_.end() || !file_.is_open()) {
		return false;
	}

	const uint64_t bytesToRead = std::min<uint64_t>(it->second.size, sample_bytes);
	std::vector<uint8_t> scratch(bytesToRead);
	file_.clear();
	file_.seekg(static_cast<std::streamoff>(data_section_offset_ + it->second.offset), std::ios::beg);
	file_.read(reinterpret_cast<char*>(scratch.data()), static_cast<std::streamsize>(bytesToRead));
	return file_.good();
}

uint64_t StreamingGGUFLoader::GetTotalFileSize() {
	if (!file_.is_open()) {
		return 0;
	}

	const std::streampos current = file_.tellg();
	file_.clear();
	file_.seekg(0, std::ios::end);
	const uint64_t size = static_cast<uint64_t>(file_.tellg());
	file_.seekg(current, std::ios::beg);
	return size;
}

bool StreamingGGUFLoader::LoadTensorZone(const std::string& tensor_name, std::vector<uint8_t>& data) {
	return GetTensorData(tensor_name, data);
}

bool StreamingGGUFLoader::LoadTensorRange(size_t start_idx, size_t count, std::vector<uint8_t>& data) {
	data.clear();
	if (start_idx >= tensor_enumeration_order_.size()) {
		return false;
	}

	const size_t end = std::min(start_idx + count, tensor_enumeration_order_.size());
	for (size_t idx = start_idx; idx < end; ++idx) {
		std::vector<uint8_t> tensorData;
		if (!GetTensorData(tensor_enumeration_order_[idx].name, tensorData)) {
			return false;
		}
		data.insert(data.end(), tensorData.begin(), tensorData.end());
	}
	return true;
}

size_t StreamingGGUFLoader::GetTensorByteSize(const TensorInfo& tensor) const {
	return static_cast<size_t>(CalculateTensorSize(tensor.shape, tensor.type));
}

std::string StreamingGGUFLoader::GetTypeString(GGMLType type) const {
	switch (type) {
	case GGMLType::F32: return "F32";
	case GGMLType::F16: return "F16";
	case GGMLType::Q4_0: return "Q4_0";
	case GGMLType::Q4_1: return "Q4_1";
	case GGMLType::Q5_0: return "Q5_0";
	case GGMLType::Q5_1: return "Q5_1";
	case GGMLType::Q8_0: return "Q8_0";
	case GGMLType::Q2_K: return "Q2_K";
	case GGMLType::Q3_K: return "Q3_K";
	case GGMLType::Q4_K: return "Q4_K";
	case GGMLType::Q5_K: return "Q5_K";
	case GGMLType::Q6_K: return "Q6_K";
	default: return "Unknown";
	}
}

uint64_t StreamingGGUFLoader::GetFileSize() const {
	if (!file_.is_open()) {
		return 0;
	}

	auto& mutableFile = const_cast<std::ifstream&>(file_);
	const std::streampos current = mutableFile.tellg();
	mutableFile.clear();
	mutableFile.seekg(0, std::ios::end);
	const uint64_t size = static_cast<uint64_t>(mutableFile.tellg());
	mutableFile.seekg(current, std::ios::beg);
	return size;
}

void StreamingGGUFLoader::AssignTensorsToZones() {
	zones_.clear();
	for (auto& [tensorName, ref] : tensor_index_) {
		std::string zone;
		if (tensorName.find("token_embd") != std::string::npos || tensorName.find("embedding") != std::string::npos) {
			zone = "embedding";
		} else if (tensorName.find("output.weight") != std::string::npos || tensorName.find("lm_head") != std::string::npos ||
				   tensorName.find("output_norm") != std::string::npos) {
			zone = "output_head";
		} else if (tensorName.find("blk.") != std::string::npos) {
			zone = "layers_" + std::to_string(ExtractLayerNumber(tensorName) / 8);
		} else {
			zone = "misc";
		}

		ref.zone_name = zone;
		auto& zoneInfo = zones_[zone];
		zoneInfo.zone_name = zone;
		zoneInfo.tensors.push_back(tensorName);
		zoneInfo.total_bytes += ref.size;
	}

	for (auto& ref : tensor_enumeration_order_) {
		const auto it = tensor_index_.find(ref.name);
		if (it != tensor_index_.end()) {
			ref.zone_name = it->second.zone_name;
		}
	}
}

bool StreamingGGUFLoader::StreamZoneFromDisk(const std::string& zone_name) {
	auto zoneIt = zones_.find(zone_name);
	if (zoneIt == zones_.end() || !file_.is_open()) {
		return false;
	}

	TensorZoneInfo& zone = zoneIt->second;
	zone.data.clear();
	zone.data.reserve(static_cast<size_t>(zone.total_bytes));

	for (const auto& tensorName : zone.tensors) {
		const auto tensorIt = tensor_index_.find(tensorName);
		if (tensorIt == tensor_index_.end()) {
			return false;
		}

		const TensorRef& ref = tensorIt->second;
		const size_t oldSize = zone.data.size();
		zone.data.resize(oldSize + static_cast<size_t>(ref.size));

		file_.clear();
		file_.seekg(static_cast<std::streamoff>(data_section_offset_ + ref.offset), std::ios::beg);
		file_.read(reinterpret_cast<char*>(zone.data.data() + oldSize), static_cast<std::streamsize>(ref.size));
		if (!file_.good()) {
			zone.data.resize(oldSize);
			return false;
		}
	}

	zone.is_loaded = true;
	current_zone_memory_ = static_cast<uint64_t>(zone.data.size());
	return true;
}

bool StreamingGGUFLoader::LoadZoneMapped(const std::string& zone_name) {
	(void)zone_name;
	return false;
}

int32_t StreamingGGUFLoader::ExtractLayerNumber(const std::string& tensor_name) const {
	const size_t start = tensor_name.find("blk.");
	if (start == std::string::npos) {
		return 0;
	}

	size_t pos = start + 4;
	size_t end = pos;
	while (end < tensor_name.size() && std::isdigit(static_cast<unsigned char>(tensor_name[end]))) {
		++end;
	}

	try {
		return std::stoi(tensor_name.substr(pos, end - pos));
	} catch (...) {
		return 0;
	}
}

std::string StreamingGGUFLoader::GetZoneForTensor(const std::string& tensor_name) const {
	const auto it = tensor_index_.find(tensor_name);
	return it != tensor_index_.end() ? it->second.zone_name : std::string{};
}

template <typename T>
bool StreamingGGUFLoader::ReadValue(T& value) {
	file_.read(reinterpret_cast<char*>(&value), sizeof(T));
	return file_.good();
}

bool StreamingGGUFLoader::ReadString(std::string& value) {
	uint64_t length = 0;
	if (!ReadValue(length)) {
		return false;
	}
	if (length > static_cast<uint64_t>(std::numeric_limits<size_t>::max())) {
		return false;
	}

	value.resize(static_cast<size_t>(length));
	if (length == 0) {
		return true;
	}

	file_.read(value.data(), static_cast<std::streamsize>(length));
	return file_.good();
}

uint64_t StreamingGGUFLoader::CalculateTensorSize(const std::vector<uint64_t>& shape, GGMLType type) const {
	uint64_t elements = 1;
	for (const uint64_t dim : shape) {
		elements *= dim;
	}

	double bytesPerElement = 4.0;
	switch (type) {
	case GGMLType::F32: bytesPerElement = 4.0; break;
	case GGMLType::F16: bytesPerElement = 2.0; break;
	case GGMLType::Q4_0:
	case GGMLType::Q4_1: bytesPerElement = 0.5; break;
	case GGMLType::Q5_0:
	case GGMLType::Q5_1: bytesPerElement = 0.625; break;
	case GGMLType::Q8_0: bytesPerElement = 1.0; break;
	case GGMLType::Q2_K: bytesPerElement = 0.3125; break;
	case GGMLType::Q3_K: bytesPerElement = 0.4375; break;
	case GGMLType::Q4_K: bytesPerElement = 0.5; break;
	case GGMLType::Q5_K: bytesPerElement = 0.625; break;
	case GGMLType::Q6_K: bytesPerElement = 0.75; break;
	default: bytesPerElement = 4.0; break;
	}

	return static_cast<uint64_t>(elements * bytesPerElement);
}

bool StreamingGGUFLoader::PrefetchZoneAsync(const std::string& zone_name, uint64_t max_memory_mb) {
	return LoadZone(zone_name, max_memory_mb);
}

} // namespace RawrXD
