import bpy
import bmesh

# Get the active mesh
me = bpy.context.object.data


# Get a BMesh representation
bm = bmesh.new()   # create an empty BMesh
bm.from_mesh(me)   # fill it in from a Mesh


# Modify the BMesh, can do anything here...
for v in bm.verts:
    v.co.x += 1.0


uv_lay = bm.loops.layers.uv.active

for face in bm.faces:
    for loop in face.loops:
        uv = loop[uv_lay].uv
        print("Loop UV: %f, %f" % uv[:])
        vert = loop.vert
        print("Loop Vert: (%f,%f,%f)" % vert.co[:])
        

# Finish up, write the bmesh back to the mesh
bm.to_mesh(me)
bm.free()  # free and prevent further access