#include "render.h"

#include <chrono>  // C++11
#include <thread>  // C++11
#include <vector> 

#include <iostream>

#include "../../nanort.h"

#include "tiny_obj_loader.h"
#include "matrix.h"
#include "trackball.h"

namespace example {

typedef struct {
  size_t num_vertices;
  size_t num_faces;
  std::vector<float> vertices;              /// [xyz] * num_vertices
  std::vector<float> facevarying_normals;   /// [xyz] * 3(triangle) * num_faces
  std::vector<float> facevarying_tangents;  /// [xyz] * 3(triangle) * num_faces
  std::vector<float> facevarying_binormals; /// [xyz] * 3(triangle) * num_faces
  std::vector<float> facevarying_uvs;       /// [xy]  * 3(triangle) * num_faces
  std::vector<float> facevarying_vertex_colors;   /// [xyz] * 3(triangle) * num_faces
  std::vector<unsigned int> faces;         /// triangle x num_faces
  std::vector<unsigned int> material_ids;   /// index x num_faces
} Mesh;

struct Material {
  float ambient[3];
  float diffuse[3];
  float reflection[3];
  float refraction[3];
  int id;
  int diffuse_texid;
  int reflection_texid;
  int transparency_texid;
  int bump_texid;
  int normal_texid;     // normal map
  int alpha_texid;      // alpha map

  Material() {
	  ambient[0] = 0.0;
	  ambient[1] = 0.0;
	  ambient[2] = 0.0;
	  diffuse[0] = 0.5;
	  diffuse[1] = 0.5;
	  diffuse[2] = 0.5;
	  reflection[0] = 0.0;
	  reflection[1] = 0.0;
	  reflection[2] = 0.0;
	  refraction[0] = 0.0;
	  refraction[1] = 0.0;
	  refraction[2] = 0.0;
	  id = -1;
    diffuse_texid = -1;
    reflection_texid = -1;
    transparency_texid = -1;
    bump_texid = -1;
    normal_texid = -1;
    alpha_texid = -1;
  }
};

Mesh gMesh;
nanort::BVHAccel<nanort::TriangleMesh, nanort::TriangleSAHPred, nanort::TriangleIntersector<> > gAccel;

void CalcNormal(nanort::float3& N, nanort::float3 v0, nanort::float3 v1, nanort::float3 v2)
{
  nanort::float3 v10 = v1 - v0;
  nanort::float3 v20 = v2 - v0;

  N = vcross(v20, v10);
  N = vnormalize(N);
}

void BuildCameraFrame(nanort::float3 *origin, nanort::float3 *corner, nanort::float3 *u,
                      nanort::float3 *v, float fov, const float quat[4],
                              int width, int height) {
  double e[4][4];

  double eye[3], lookat[3], up[3];
  Matrix::LookAt(e, eye, lookat, up);

  float r0[4][4];
  double r[4][4];
  build_rotmatrix(r0, quat);
  Matrix::FloatToDoubleMatrix(r, r0);

  nanort::float3 lo;
  lo[0] = lookat[0] - eye[0];
  lo[1] = lookat[1] - eye[1];
  lo[2] = lookat[2] - eye[2];
  double dist = vlength(lo);

  double dir[3];
  dir[0] = 0.0;
  dir[1] = 0.0;
  dir[2] = dist;

  Matrix::Inverse(r);

  double rr[4][4];
  double re[4][4];
  double zero[3] = {0.0f, 0.0f, 0.0f};
  double localUp[3] = {0.0f, 1.0f, 0.0f};
  Matrix::LookAt(re, dir, zero, localUp);

  // translate
  re[3][0] += eye[0]; // 0.0; //lo[0];
  re[3][1] += eye[1]; // 0.0; //lo[1];
  re[3][2] += (eye[2] - dist);

  // rot -> trans
  Matrix::Mult(rr, r, re);

  double m[4][4];
  for (int j = 0; j < 4; j++) {
    for (int i = 0; i < 4; i++) {
      m[j][i] = rr[j][i];
    }
  }

  double vzero[3] = {0.0f, 0.0f, 0.0f};
  double eye1[3];
  Matrix::MultV(eye1, m, vzero);

  double lookat1d[3];
  dir[2] = -dir[2];
  Matrix::MultV(lookat1d, m, dir);
  nanort::float3 lookat1(lookat1d[0], lookat1d[1], lookat1d[2]);

  double up1d[3];
  Matrix::MultV(up1d, m, up);

  nanort::float3 up1(up1d[0], up1d[1], up1d[2]);

  // absolute -> relative
  up1[0] -= eye1[0];
  up1[1] -= eye1[1];
  up1[2] -= eye1[2];
  // printf("up1(after) = %f, %f, %f\n", up1[0], up1[1], up1[2]);

  // Use original up vector
  up1[0] = up[0];
  up1[1] = up[1];
  up1[2] = up[2];

  {
    double flen =
        (0.5f * (double)height / tanf(0.5f * (double)(fov * M_PI / 180.0f)));
    nanort::float3 look1;
    look1[0] = lookat1[0] - eye1[0];
    look1[1] = lookat1[1] - eye1[1];
    look1[2] = lookat1[2] - eye1[2];
    // vcross(u, up1, look1);
    // flip
    (*u) = nanort::vcross(look1, up1);
    (*u) = vnormalize((*u));

    (*v) = vcross(look1, (*u));
    (*v) = vnormalize((*v));

    look1 = vnormalize(look1);
    look1[0] = flen * look1[0] + eye1[0];
    look1[1] = flen * look1[1] + eye1[1];
    look1[2] = flen * look1[2] + eye1[2];
    (*corner)[0] = look1[0] - 0.5f * (width * (*u)[0] + height * (*v)[0]);
    (*corner)[1] = look1[1] - 0.5f * (width * (*u)[1] + height * (*v)[1]);
    (*corner)[2] = look1[2] - 0.5f * (width * (*u)[2] + height * (*v)[2]);

    (*origin)[0] = eye1[0];
    (*origin)[1] = eye1[1];
    (*origin)[2] = eye1[2];

  }
}

nanort::Ray GenerateRay(const nanort::float3& origin, const nanort::float3& corner, const nanort::float3& du, const nanort::float3& dv, float u, float v) 
{
  nanort::float3 dir;

  dir[0] = (corner[0] + u * du[0] + v * dv[0]) - origin[0];
  dir[1] = (corner[1] + u * du[1] + v * dv[1]) - origin[1];
  dir[2] = (corner[2] + u * du[2] + v * dv[2]) - origin[2];
  dir = vnormalize(dir);

  nanort::float3 org;

  nanort::Ray ray;
  ray.org[0] = origin[0];
  ray.org[1] = origin[1];
  ray.org[2] = origin[2];
  ray.dir[0] = dir[0];

  return ray;
}


bool LoadObj(Mesh &mesh, const char *filename, float scale) {
  std::vector<tinyobj::shape_t> shapes;
  std::vector<tinyobj::material_t> materials;

  std::string err = tinyobj::LoadObj(shapes, materials, filename);

  if (!err.empty()) {
    std::cerr << err << std::endl;
    return false;
  }

  std::cout << "[LoadOBJ] # of shapes in .obj : " << shapes.size() << std::endl;
  std::cout << "[LoadOBJ] # of materials in .obj : " << materials.size() << std::endl;

  size_t num_vertices = 0;
  size_t num_faces = 0;
  for (size_t i = 0; i < shapes.size(); i++) {
    printf("  shape[%ld].name = %s\n", i, shapes[i].name.c_str());
    printf("  shape[%ld].indices: %ld\n", i, shapes[i].mesh.indices.size());
    assert((shapes[i].mesh.indices.size() % 3) == 0);
    printf("  shape[%ld].vertices: %ld\n", i, shapes[i].mesh.positions.size());
    assert((shapes[i].mesh.positions.size() % 3) == 0);
    printf("  shape[%ld].normals: %ld\n", i, shapes[i].mesh.normals.size());
    assert((shapes[i].mesh.normals.size() % 3) == 0);

    num_vertices += shapes[i].mesh.positions.size() / 3;
    num_faces += shapes[i].mesh.indices.size() / 3;
  }
  std::cout << "[LoadOBJ] # of faces: " << num_faces << std::endl;
  std::cout << "[LoadOBJ] # of vertices: " << num_vertices << std::endl;

  // @todo { material and texture. }

  // Shape -> Mesh
  mesh.num_faces = num_faces;
  mesh.num_vertices = num_vertices;
  mesh.vertices.resize(num_vertices * 3, 0.0f);
  mesh.faces.resize(num_faces * 3, 0);
  mesh.material_ids.resize(num_faces, 0);
  mesh.facevarying_normals.resize(num_faces * 3 * 3, 0.0f);
  mesh.facevarying_uvs.resize(num_faces * 3 * 2, 0.0f);

  // @todo {}
  //mesh.facevarying_tangents = NULL;
  //mesh.facevarying_binormals = NULL;

  size_t vertexIdxOffset = 0;
  size_t faceIdxOffset = 0;
  for (size_t i = 0; i < shapes.size(); i++) {

    for (size_t f = 0; f < shapes[i].mesh.indices.size() / 3; f++) {
      mesh.faces[3 * (faceIdxOffset + f) + 0] =
          shapes[i].mesh.indices[3 * f + 0];
      mesh.faces[3 * (faceIdxOffset + f) + 1] =
          shapes[i].mesh.indices[3 * f + 1];
      mesh.faces[3 * (faceIdxOffset + f) + 2] =
          shapes[i].mesh.indices[3 * f + 2];

      mesh.faces[3 * (faceIdxOffset + f) + 0] += vertexIdxOffset;
      mesh.faces[3 * (faceIdxOffset + f) + 1] += vertexIdxOffset;
      mesh.faces[3 * (faceIdxOffset + f) + 2] += vertexIdxOffset;

      mesh.material_ids[faceIdxOffset + f] = shapes[i].mesh.material_ids[f];
    }

    for (size_t v = 0; v < shapes[i].mesh.positions.size() / 3; v++) {
      mesh.vertices[3 * (vertexIdxOffset + v) + 0] =
          scale * shapes[i].mesh.positions[3 * v + 0];
      mesh.vertices[3 * (vertexIdxOffset + v) + 1] =
          scale * shapes[i].mesh.positions[3 * v + 1];
      mesh.vertices[3 * (vertexIdxOffset + v) + 2] =
          scale * shapes[i].mesh.positions[3 * v + 2];
    }

    if (shapes[i].mesh.normals.size() > 0) {
      for (size_t f = 0; f < shapes[i].mesh.indices.size() / 3; f++) {
        int f0, f1, f2;

        f0 = shapes[i].mesh.indices[3*f+0];
        f1 = shapes[i].mesh.indices[3*f+1];
        f2 = shapes[i].mesh.indices[3*f+2];

        nanort::float3 n0, n1, n2;

        n0[0] = shapes[i].mesh.normals[3 * f0 + 0];
        n0[1] = shapes[i].mesh.normals[3 * f0 + 1];
        n0[2] = shapes[i].mesh.normals[3 * f0 + 2];

        n1[0] = shapes[i].mesh.normals[3 * f1 + 0];
        n1[1] = shapes[i].mesh.normals[3 * f1 + 1];
        n1[2] = shapes[i].mesh.normals[3 * f1 + 2];

        n2[0] = shapes[i].mesh.normals[3 * f2 + 0];
        n2[1] = shapes[i].mesh.normals[3 * f2 + 1];
        n2[2] = shapes[i].mesh.normals[3 * f2 + 2];

        mesh.facevarying_normals[3 * (3 * (faceIdxOffset + f) + 0) + 0] = n0[0];
        mesh.facevarying_normals[3 * (3 * (faceIdxOffset + f) + 0) + 1] = n0[1];
        mesh.facevarying_normals[3 * (3 * (faceIdxOffset + f) + 0) + 2] = n0[2];

        mesh.facevarying_normals[3 * (3 * (faceIdxOffset + f) + 1) + 0] = n1[0];
        mesh.facevarying_normals[3 * (3 * (faceIdxOffset + f) + 1) + 1] = n1[1];
        mesh.facevarying_normals[3 * (3 * (faceIdxOffset + f) + 1) + 2] = n1[2];

        mesh.facevarying_normals[3 * (3 * (faceIdxOffset + f) + 2) + 0] = n2[0];
        mesh.facevarying_normals[3 * (3 * (faceIdxOffset + f) + 2) + 1] = n2[1];
        mesh.facevarying_normals[3 * (3 * (faceIdxOffset + f) + 2) + 2] = n2[2];
      }
    } else {
      // calc geometric normal
      for (size_t f = 0; f < shapes[i].mesh.indices.size() / 3; f++) {
        int f0, f1, f2;

        f0 = shapes[i].mesh.indices[3*f+0];
        f1 = shapes[i].mesh.indices[3*f+1];
        f2 = shapes[i].mesh.indices[3*f+2];

        nanort::float3 v0, v1, v2;

        v0[0] = shapes[i].mesh.positions[3 * f0 + 0];
        v0[1] = shapes[i].mesh.positions[3 * f0 + 1];
        v0[2] = shapes[i].mesh.positions[3 * f0 + 2];

        v1[0] = shapes[i].mesh.positions[3 * f1 + 0];
        v1[1] = shapes[i].mesh.positions[3 * f1 + 1];
        v1[2] = shapes[i].mesh.positions[3 * f1 + 2];

        v2[0] = shapes[i].mesh.positions[3 * f2 + 0];
        v2[1] = shapes[i].mesh.positions[3 * f2 + 1];
        v2[2] = shapes[i].mesh.positions[3 * f2 + 2];

        nanort::float3 N;
        CalcNormal(N, v0, v1, v2);

        mesh.facevarying_normals[3 * (3 * (faceIdxOffset + f) + 0) + 0] = N[0];
        mesh.facevarying_normals[3 * (3 * (faceIdxOffset + f) + 0) + 1] = N[1];
        mesh.facevarying_normals[3 * (3 * (faceIdxOffset + f) + 0) + 2] = N[2];

        mesh.facevarying_normals[3 * (3 * (faceIdxOffset + f) + 1) + 0] = N[0];
        mesh.facevarying_normals[3 * (3 * (faceIdxOffset + f) + 1) + 1] = N[1];
        mesh.facevarying_normals[3 * (3 * (faceIdxOffset + f) + 1) + 2] = N[2];

        mesh.facevarying_normals[3 * (3 * (faceIdxOffset + f) + 2) + 0] = N[0];
        mesh.facevarying_normals[3 * (3 * (faceIdxOffset + f) + 2) + 1] = N[1];
        mesh.facevarying_normals[3 * (3 * (faceIdxOffset + f) + 2) + 2] = N[2];

      }

    }

    if (shapes[i].mesh.texcoords.size() > 0) {
      for (size_t f = 0; f < shapes[i].mesh.indices.size() / 3; f++) {
        int f0, f1, f2;

        f0 = shapes[i].mesh.indices[3*f+0];
        f1 = shapes[i].mesh.indices[3*f+1];
        f2 = shapes[i].mesh.indices[3*f+2];

        nanort::float3 n0, n1, n2;

        n0[0] = shapes[i].mesh.texcoords[2 * f0 + 0];
        n0[1] = shapes[i].mesh.texcoords[2 * f0 + 1];

        n1[0] = shapes[i].mesh.texcoords[2 * f1 + 0];
        n1[1] = shapes[i].mesh.texcoords[2 * f1 + 1];

        n2[0] = shapes[i].mesh.texcoords[2 * f2 + 0];
        n2[1] = shapes[i].mesh.texcoords[2 * f2 + 1];

        mesh.facevarying_uvs[2 * (3 * (faceIdxOffset + f) + 0) + 0] = n0[0];
        mesh.facevarying_uvs[2 * (3 * (faceIdxOffset + f) + 0) + 1] = n0[1];

        mesh.facevarying_uvs[2 * (3 * (faceIdxOffset + f) + 1) + 0] = n1[0];
        mesh.facevarying_uvs[2 * (3 * (faceIdxOffset + f) + 1) + 1] = n1[1];

        mesh.facevarying_uvs[2 * (3 * (faceIdxOffset + f) + 2) + 0] = n2[0];
        mesh.facevarying_uvs[2 * (3 * (faceIdxOffset + f) + 2) + 1] = n2[1];
      }
    }

    vertexIdxOffset += shapes[i].mesh.positions.size() / 3;
    faceIdxOffset += shapes[i].mesh.indices.size() / 3;
  }

  return true;
}

bool Renderer::LoadObjMesh(const char* obj_filename, float scene_scale)
{
  bool ret =  LoadObj(gMesh, obj_filename, scene_scale);
  if (!ret) return false;

  nanort::BVHBuildOptions build_options; // Use default option
  build_options.cache_bbox = false;

  printf("  BVH build option:\n");
  printf("    # of leaf primitives: %d\n", build_options.min_leaf_primitives);
  printf("    SAH binsize         : %d\n", build_options.bin_size);

  auto t_start = std::chrono::system_clock::now();

  nanort::TriangleMesh triangle_mesh(gMesh.vertices.data(), gMesh.faces.data());
  nanort::TriangleSAHPred triangle_pred(gMesh.vertices.data(), gMesh.faces.data());

  printf("num_triangles = %lu\n", gMesh.num_faces);

  ret = gAccel.Build(gMesh.num_faces, build_options, triangle_mesh, triangle_pred);
  assert(ret);

  auto t_end = std::chrono::system_clock::now();

  std::chrono::duration<double, std::milli> ms = t_end - t_start;
  std::cout << "BVH build time: " << ms.count() << " [ms]\n";

  nanort::BVHBuildStatistics stats = gAccel.GetStatistics();

  printf("  BVH statistics:\n");
  printf("    # of leaf   nodes: %d\n", stats.num_leaf_nodes);
  printf("    # of branch nodes: %d\n", stats.num_branch_nodes);
  printf("  Max tree depth     : %d\n", stats.max_tree_depth);
  float bmin[3], bmax[3];
  gAccel.BoundingBox(bmin, bmax);
  printf("  Bmin               : %f, %f, %f\n", bmin[0], bmin[1], bmin[2]);
  printf("  Bmax               : %f, %f, %f\n", bmax[0], bmax[1], bmax[2]);


  return true;

}

bool Renderer::Render(float* rgba, float *aux_rgba, const RenderConfig& config, std::atomic<bool>& cancelFlag)
{
  if (!gAccel.IsValid()) {
    return false;
  }

  auto kCancelFlagCheckMilliSeconds = 300;

  std::vector<std::thread> workers;
  std::atomic<int> i( 0 );

  uint32_t num_threads = std::max( 1U, std::thread::hardware_concurrency() );

  auto startT = std::chrono::system_clock::now();

  for( auto t = 0; t < num_threads; t++ )
  {
    workers.push_back( std::thread( [&, t]() {

      int y = 0;
      while ((y = i++) < config.height) {
        auto currT = std::chrono::system_clock::now();

        std::chrono::duration<double, std::milli> ms = currT - startT;
        // Check cancel flag
        if (ms.count() > kCancelFlagCheckMilliSeconds) {
          if (cancelFlag) {
            break;
          }
        }

        // draw dash line to aux buffer for progress.
        for (int x = 0; x < config.width; x++) {
          float c = (x / 8) % 2;
          aux_rgba[4*(y*config.width+x)+0] = c;
          aux_rgba[4*(y*config.width+x)+1] = c;
          aux_rgba[4*(y*config.width+x)+2] = c;
          aux_rgba[4*(y*config.width+x)+3] = 0.0f;
        }

        //std::this_thread::sleep_for( std::chrono::milliseconds( 5 ) );

        for (int x = 0; x < config.width; x++) {

          nanort::Ray ray;
          ray.org[0] = config.eye[0];
          ray.org[1] = config.eye[1];
          ray.org[2] = config.eye[2];

          nanort::float3 dir;
          dir[0] = (x / (float)config.width) - 0.5f;
          dir[1] = (y / (float)config.height) - 0.5f;
          dir[2] = -1.0f;
          dir = vnormalize(dir);
          ray.dir[0] = dir[0];
          ray.dir[1] = dir[1];
          ray.dir[2] = dir[2];

          float kFar = 1.0e+30f;
          ray.min_t = 0.0f;
          ray.max_t = kFar;

          nanort::TriangleIntersector<> triangle_intersector(gMesh.vertices.data(), gMesh.faces.data());
          nanort::BVHTraceOptions trace_options;
          bool hit = gAccel.Traverse(ray, trace_options, triangle_intersector);
          if (hit) {
            rgba[4*(y*config.width+x)+0] = x / static_cast<float>(config.width);
            rgba[4*(y*config.width+x)+1] = y / static_cast<float>(config.height);
            rgba[4*(y*config.width+x)+2] = config.pass / static_cast<float>(config.max_passes);
            rgba[4*(y*config.width+x)+3] = 1.0f;

            nanort::float3 p;
            p[0] = ray.org[0] + triangle_intersector.intersection.t * ray.dir[0];
            p[1] = ray.org[1] + triangle_intersector.intersection.t * ray.dir[1];
            p[2] = ray.org[2] + triangle_intersector.intersection.t * ray.dir[2];

            config.positionImage[4*(y*config.width+x)+0] = p.x();
            config.positionImage[4*(y*config.width+x)+1] = p.y();
            config.positionImage[4*(y*config.width+x)+2] = p.z();
            config.positionImage[4*(y*config.width+x)+3] = 1.0f;

            config.varycoordImage[4*(y*config.width+x)+0] = triangle_intersector.intersection.u;
            config.varycoordImage[4*(y*config.width+x)+1] = triangle_intersector.intersection.v;
            config.varycoordImage[4*(y*config.width+x)+2] = 0.0f;
            config.varycoordImage[4*(y*config.width+x)+3] = 1.0f;

            unsigned int prim_id = triangle_intersector.intersection.prim_id;

            if (gMesh.facevarying_normals.size() > 0) {
              // @fixme { interpolate. }
              config.normalImage[4 * (y*config.width+x)+0] = 0.5 * gMesh.facevarying_normals[9 * prim_id + 0] + 0.5;
              config.normalImage[4 * (y*config.width+x)+1] = 0.5 * gMesh.facevarying_normals[9 * prim_id + 1] + 0.5;
              config.normalImage[4 * (y*config.width+x)+2] = 0.5 * gMesh.facevarying_normals[9 * prim_id + 2] + 0.5;
              config.normalImage[4 * (y*config.width+x)+3] = 1.0f;
            }

            if (gMesh.facevarying_uvs.size() > 0) {
              // @fixme { interpolate }
              config.texcoordImage[4 * (y*config.width+x)+0] = gMesh.facevarying_uvs[6 * prim_id + 0];
              config.texcoordImage[4 * (y*config.width+x)+1] = gMesh.facevarying_uvs[6 * prim_id + 1];

            }
          }
        }

        for (int x = 0; x < config.width; x++) {
          aux_rgba[4*(y*config.width+x)+0] = 0.0f;
          aux_rgba[4*(y*config.width+x)+1] = 0.0f;
          aux_rgba[4*(y*config.width+x)+2] = 0.0f;
          aux_rgba[4*(y*config.width+x)+3] = 0.0f;
        }
      }
    }));
  }

  for (auto &t : workers) {
    t.join();
  }

  return true;
  
};

}  // namespace example