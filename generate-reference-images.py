#!/usr/bin/env python3
import bpy, sys

argv = sys.argv
argv = argv[argv.index("--") + 1:]
model_path, output_path = argv

# clean scene
bpy.ops.wm.read_factory_settings(use_empty=True)

# import
bpy.ops.import_scene.gltf(filepath=model_path)

scene = bpy.context.scene
scene.render.engine = 'BLENDER_EEVEE'

# make it stable / predictable
scene.eevee.taa_render_samples = 1
#scene.eevee.use_gtao = False
#scene.eevee.use_bloom = False
#scene.eevee.use_ssr = False

# color management (important)
scene.view_settings.view_transform = 'Standard'
scene.view_settings.look = 'None'
scene.view_settings.exposure = 0
scene.view_settings.gamma = 1

# camera
cam = bpy.data.cameras.new("cam")
cam.angle = 60 * 3.14159265 / 180.0
cam_obj = bpy.data.objects.new("cam", cam)
scene.collection.objects.link(cam_obj)
scene.camera = cam_obj

import mathutils

objs = [o for o in scene.objects if o.type == 'MESH']
min_v = mathutils.Vector((1e9, 1e9, 1e9))
max_v = mathutils.Vector((-1e9, -1e9, -1e9))

for o in objs:
    for v in o.bound_box:
        wv = o.matrix_world @ mathutils.Vector(v)
        min_v = mathutils.Vector((min(min_v[i], wv[i]) for i in range(3)))
        max_v = mathutils.Vector((max(max_v[i], wv[i]) for i in range(3)))

print("min_v:", min_v)
print("max_v:", max_v)

center = (min_v + max_v) / 2
cam_obj.location = center + mathutils.Vector((0, 0, max_v.z * 1.5))
direction = center - cam_obj.location
cam_obj.rotation_euler = direction.to_track_quat('-Z', 'Y').to_euler()
print("Camera location:", cam_obj.location)
print("center:", center)

# light
#light = bpy.data.lights.new(name="sun", type='SUN')
#light.energy = 3.0
#light_obj = bpy.data.objects.new("sun", light)
#scene.collection.objects.link(light_obj)
#light_obj.rotation_euler = (0.7, 0.5, 0)

# sanity remove all lights
for obj in scene.objects:
 if obj.type == 'LIGHT':
  bpy.data.objects.remove(obj, do_unlink=True)

# scene.world.use_nodes = True
if scene.world is None:
	 scene.world = bpy.data.worlds.new("World")
scene.world.use_nodes = True
nodes = scene.world.node_tree.nodes
bg = nodes.get("Background")
bg.inputs[0].default_value = (1.0, 1.0, 1.0, 1.0)
bg.inputs[1].default_value = 1.0

# resolution
scene.render.resolution_x = 512
scene.render.resolution_y = 512
scene.render.filepath = output_path

bpy.ops.render.render(write_still=True)
