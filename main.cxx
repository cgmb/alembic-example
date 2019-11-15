#include <cstdarg>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cinttypes>
#include <cstdio>
#include <array>
#include <fstream>
#include <sstream>
#include <iostream>
#include <string>
#include <vector>

#include <Alembic/AbcGeom/All.h>
#include <Alembic/AbcCoreOgawa/All.h>

using std::uint32_t;
using std::int32_t;
using std::size_t;

using vec3f = std::array<float,3>;

struct Mesh {
  std::vector<vec3f> vertexes;
  std::vector<int32_t> indexes;
  std::vector<int32_t> faces;
};

std::ostream& readerr(const char* filename, size_t line_number) {
  return std::cerr << filename << ':' << line_number << ':';
}

bool idx_ok(int32_t idx, size_t vertex_count) {
  return idx > 0 && size_t(idx) <= vertex_count;
}

bool starts_with(const char* s, const char* p) {
  while (*p) {
    if (*s++ != *p++) {
      return false;
    }
  }
  return true;
}

void die(const char* fmt, ...) {
  std::va_list argp;
  va_start(argp, fmt);
  std::vfprintf(stderr, fmt, argp);
  va_end(argp);
  std::exit(EXIT_FAILURE);
}

std::string read_file(const char* filename) {
  std::ifstream in(filename, std::ios::binary | std::ios::ate);
  const auto filesize = in.tellg();
  in.seekg(0);
  std::string str(filesize, '\0');
  if (!in.read(&str[0], filesize)) {
    die("%s: Could not read file\n", filename);
  }
  return str;
}

bool find_end_of(const std::string& str, const std::string& needle, size_t* result) {
  const size_t pos = str.find(needle);
  if (pos == std::string::npos) {
    return false;
  }
  *result = pos + needle.size();
  return true;
}

struct PlyHeader {
  size_t vertex_count = 0;
  size_t face_count = 0;

  static const size_t vertex_size = 12;
};

PlyHeader parse_ply_header(const std::string& s, const char* filename) {
  PlyHeader ph;

  std::istringstream ss;
  ss.str(s);

  enum {
    EXPECT_MAGIC,
    EXPECT_FORMAT,
    EXPECT_VERTEX_ELEMENT,
    EXPECT_VERTEX_X,
    EXPECT_VERTEX_Y,
    EXPECT_VERTEX_Z,
    EXPECT_FACE_ELEMENT,
    EXPECT_FACE_VERTEX_INDEX,
    EXPECT_END_HEADER,
    EXPECT_DATA,
  } parser_expects = EXPECT_MAGIC;

  std::string line;
  size_t line_number = 0;
  while (std::getline(ss, line)) {
    ++line_number;
    switch (parser_expects) {
      case EXPECT_MAGIC: {
        if (line == "ply") {
          parser_expects = EXPECT_FORMAT;
        } else {
          readerr(filename, line_number) << "not a PLY file" << std::endl;
        }
      } break;
      case EXPECT_FORMAT: {
        if (line == "format binary_little_endian 1.0") {
          parser_expects = EXPECT_VERTEX_ELEMENT;
        } else {
          readerr(filename, line_number) << "unsupported format" << std::endl;
        }
      } break;
      case EXPECT_VERTEX_ELEMENT: {
        if (1 == std::sscanf(line.c_str(), "element vertex %zu", &ph.vertex_count)) {
          parser_expects = EXPECT_VERTEX_X;
        } else {
          readerr(filename, line_number) << "unsupported vertex element" << std::endl;
        }
      } break;
      case EXPECT_VERTEX_X: {
        if (line == "property float x") {
          parser_expects = EXPECT_VERTEX_Y;
        } else {
          readerr(filename, line_number) << "unsupported vertex property" << std::endl;
        }
      } break;
      case EXPECT_VERTEX_Y: {
        if (line == "property float y") {
          parser_expects = EXPECT_VERTEX_Z;
        } else {
          readerr(filename, line_number) << "unsupported vertex property" << std::endl;
        }
      } break;
      case EXPECT_VERTEX_Z: {
        if (line == "property float z") {
          parser_expects = EXPECT_FACE_ELEMENT;
        } else {
          readerr(filename, line_number) << "unsupported vertex property" << std::endl;
        }
      } break;
      case EXPECT_FACE_ELEMENT: {
        if (1 == std::sscanf(line.c_str(), "element face %zu", &ph.face_count)) {
          parser_expects = EXPECT_FACE_VERTEX_INDEX;
        } else {
          readerr(filename, line_number) << "unsupported face element" << std::endl;
        }
      } break;
      case EXPECT_FACE_VERTEX_INDEX: {
        if (line == "property list uchar uint vertex_index") {
          parser_expects = EXPECT_END_HEADER;
        } else {
          readerr(filename, line_number) << "unsupported vertex_index property" << std::endl;
        }
      } break;
      case EXPECT_END_HEADER: {
        if (line == "end_header") {
          parser_expects = EXPECT_DATA;
        } else {
          readerr(filename, line_number) << "unsupported field" << std::endl;
        }
      } break;
      case EXPECT_DATA: {
        // std::getline would have returned false if end_header was correctly
        // followed by a newline character
        readerr(filename, line_number) << "missing newline after end_header" << std::endl;
      } break;
    }
  }

  if (!line.empty()) {
    readerr(filename, line_number) << "missing newline after end_header" << std::endl;
  }

  return ph;
}

bool is_mul_safe(size_t x, size_t y) {
  return y == 0 || x*y/y == x;
}

bool ply_idx_ok(uint32_t index, size_t vertex_count) {
  return index < uint32_t(INT32_MAX) && size_t(index) < vertex_count;
}

Mesh load_ply(const char* filename) {
  Mesh m;
  std::string s = read_file(filename);

  size_t header_size;
  if (!find_end_of(s, "end_header\n", &header_size)) {
    die("%s: Couldn't find 'end_header\\n'\n", filename);
  }

  const char* const header_begin = s.data();
//const char* const header_end = s.data() + header_size;
  const char* const data_begin = s.data() + header_size;
  const char* const data_end = s.data() + s.size();

  PlyHeader ph = parse_ply_header(std::string(header_begin, header_size), filename);

  // extract the vertex data
  if (!is_mul_safe(ph.vertex_size, ph.vertex_count)) {
    die("%s: Vertex count too large\n", filename);
  }
  size_t vertex_data_size = ph.vertex_size * ph.vertex_count;
  size_t actual_data_size = size_t(data_end - data_begin);
  if (actual_data_size < vertex_data_size) {
    die("%s: Expected %zu bytes of vertex data but got %zu\n", filename,
      vertex_data_size, actual_data_size);
  }
  m.vertexes.resize(ph.vertex_count);
  std::memcpy(m.vertexes.data(), data_begin, vertex_data_size);

  // extract the index data
  const char* rpos = data_begin + vertex_data_size;
  for (size_t i = 0; i < ph.face_count; ++i) {
    if (data_end - rpos < 1) {
      die("%s: Expected %zu faces but got %zu\n", filename, ph.face_count, i);
    }
    uint8_t index_count;
    std::memcpy(&index_count, rpos, 1);
    rpos += 1;
    m.faces.push_back(index_count);
    for (uint8_t i = 0; i < index_count; ++i) {
      if (data_end - rpos < 4) {
        die("%s: Expected index but reached end of file\n", filename);
      }
      uint32_t index;
      std::memcpy(&index, rpos, 4);
      rpos += 4;
      if (!ply_idx_ok(index, ph.vertex_count)) {
        die("%s: Invalid index (%" PRIu32 ")\n", filename, index);
      }
      m.indexes.push_back(index);
    }
  }

  if (rpos != data_end) {
    std::fprintf(stderr, "%s: Extra %td bytes at end of file\n", filename, data_end - rpos);
  }

  return m;
}

Mesh load_obj(const char* filename) {
  Mesh m;
  std::ifstream in(filename);
  std::string line;
  size_t line_number = 0;
  while (std::getline(in, line)) {
    ++line_number;
    if (line.empty()) {
      continue;
    }

    if (starts_with(line.c_str(), "v ")) {
      float x, y, z;
      if(3 == std::sscanf(line.c_str(), "v %f %f %f", &x, &y, &z)) {
        vec3f vertex = {x, y, z};
        m.vertexes.push_back(vertex);
      } else {
        readerr(filename, line_number) << "not a recognized vertex format" << std::endl;
      }
    } else if (starts_with(line.c_str(), "f ")) {
      int32_t i, j, k, l;
      if (4 == std::sscanf(line.c_str(), "f %" SCNd32 " %" SCNd32 " %" SCNd32 " %" SCNd32, &i, &j, &k, &l)) {
        const size_t vcount = m.vertexes.size();
        if (idx_ok(i, vcount) && idx_ok(j, vcount) && idx_ok(k, vcount) && idx_ok(l, vcount)) {
          m.faces.push_back(4);
          m.indexes.push_back(i-1);
          m.indexes.push_back(j-1);
          m.indexes.push_back(k-1);
          m.indexes.push_back(l-1);
        } else {
          readerr(filename, line_number) << "invalid index" << std::endl;
        }
      } else if (3 == std::sscanf(line.c_str(), "f %" SCNd32 " %" SCNd32 " %" SCNd32, &i, &j, &k)) {
        const size_t vcount = m.vertexes.size();
        if (idx_ok(i, vcount) && idx_ok(j, vcount) && idx_ok(k, vcount)) {
          m.faces.push_back(3);
          m.indexes.push_back(i-1);
          m.indexes.push_back(j-1);
          m.indexes.push_back(k-1);
        } else {
          readerr(filename, line_number) << "invalid index" << std::endl;
        }
      } else {
        readerr(filename, line_number) << "not a valid index format" << std::endl;
      }
    }
  }
  return m;
}

namespace AA = Alembic::Abc;
namespace AAG = Alembic::AbcGeom;

struct AlembicExportParameters {
  std::string application_name;
  std::string scene_description;
  std::string object_name;
  double fps;
};

void set_property(AAG::OPolyMeshSchema& schema, const char* name, bool value) {
  AA::OCompoundProperty container = schema.getUserProperties();
  AA::OBoolProperty property(container, name);
  property.set(value);
}

void export_to_alembic(std::ostream& out,
  const AlembicExportParameters& params, const std::vector<Mesh>& meshes) {
  AA::MetaData meta;
  meta.set(AA::kApplicationNameKey, params.application_name);
  meta.set(AA::kUserDescriptionKey, params.scene_description);
  meta.set(AA::kDCCFPSKey, std::to_string(params.fps));

  Alembic::AbcCoreOgawa::WriteArchive writer;
  AA::OArchive archive(writer(&out, meta), AA::ErrorHandler::kThrowPolicy);

  uint32_t time_sampling_idx;
  { // 'uniform' time sampling
    AA::chrono_t time_per_cycle = 1.0 / params.fps;
    AA::chrono_t start_time = 0.0;
    time_sampling_idx = archive.addTimeSampling(AA::TimeSampling(time_per_cycle, start_time));
  }
  AAG::OXform xform(AAG::OObject(archive, AAG::kTop), "root_transform", time_sampling_idx);

  AAG::OPolyMesh omesh(xform, params.object_name, time_sampling_idx);
  AAG::OPolyMeshSchema& schema = omesh.getSchema();
  {
    bool is_subdivision_surface = false;
    set_property(schema, "meshtype", is_subdivision_surface);
  }
  for (const Mesh& m : meshes) {
    // Note: Alembic uses a clockwise winding order
    AAG::OPolyMeshSchema::Sample sample(
      AAG::V3fArraySample((const AAG::V3f*)m.vertexes.data(), m.vertexes.size()),
      AAG::Int32ArraySample(m.indexes.data(), m.indexes.size()),
      AAG::Int32ArraySample(m.faces.data(), m.faces.size()));
    schema.set(sample);
  }
}

int main(int argc, const char** argv) {
  AlembicExportParameters parameters;
  parameters.application_name = "AlEx";
  parameters.scene_description = "An example mesh animation for Blender.";
  parameters.object_name = "exobj";
  parameters.fps = 24.0;

  std::ofstream out("out.abc", std::ios::binary);
  std::vector<Mesh> meshes;
  for (int i = 1; i < argc; ++i) {
    const char* filename = argv[i];
    const char* ext = std::strrchr(filename, '.');
    if (ext && std::strcmp(ext, ".ply") == 0) {
      meshes.push_back(load_ply(filename));
    } else if (ext && std::strcmp(ext, ".obj") == 0) {
      meshes.push_back(load_obj(filename));
    } else {
      std::fprintf(stderr, "Unknown file type: %s\n", filename);
      return 1;
    }
  }
  export_to_alembic(out, parameters, meshes);
  return 0;
}
