#include <cstdint>
#include <cinttypes>
#include <cstdio>
#include <array>
#include <fstream>
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

  Alembic::AbcCoreOgawa::WriteArchive writer;
  AA::OArchive archive(writer(&out, meta), AA::ErrorHandler::kThrowPolicy);

  uint32_t time_sampling_idx;
  { // 'uniform' time sampling
    AA::chrono_t time_per_cycle = 1.0 / 24.0;
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

  std::ofstream out("out.abc", std::ios::binary);
  std::vector<Mesh> meshes;
  for (int i = 1; i < argc; ++i) {
    meshes.push_back(load_obj(argv[i]));
  }
  export_to_alembic(out, parameters, meshes);
  return 0;
}
