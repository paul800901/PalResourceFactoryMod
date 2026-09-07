"""Author original PRF hard-surface assets using Blender 4.1+. No game files touched.

Run with Blender --background --factory-startup --python this_file.py.
Outputs are beside this script. Dimensions are metres; front is -Y, up is +Z.
"""
from pathlib import Path
from math import pi, sin, cos, radians
import json
import bpy
from mathutils import Vector

OUT = Path(__file__).resolve().parent
for folder in ('models', 'previews'):
    (OUT / folder).mkdir(exist_ok=True)
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)
for c in list(bpy.data.collections):
    bpy.data.collections.remove(c)
SCENE = bpy.context.scene
SCENE.unit_settings.system = 'METRIC'
SCENE.unit_settings.scale_length = 1.0
SCENE.render.engine = 'CYCLES'
SCENE.cycles.device = 'CPU'
SCENE.cycles.samples = 40
SCENE.cycles.use_denoising = True
SCENE.render.threads_mode = 'FIXED'
SCENE.render.threads = 8
SCENE.render.resolution_percentage = 100
SCENE.render.image_settings.file_format = 'PNG'
SCENE.view_settings.view_transform = 'AgX'
SCENE.render.film_transparent = False
SCENE.world.color = (0.12, 0.12, 0.12)
SCENE.world.use_nodes = True
SCENE.world.node_tree.nodes['Background'].inputs['Color'].default_value = (0.17, 0.23, 0.3, 1)
SCENE.world.node_tree.nodes['Background'].inputs['Strength'].default_value = 0.35
CURRENT = None


def material(name, color, metal=0, rough=0.4, emission=0):
    m = bpy.data.materials.new(name)
    m.diffuse_color = (*color, 1)
    m.use_nodes = True
    p = m.node_tree.nodes.get('Principled BSDF')
    p.inputs['Base Color'].default_value = (*color, 1)
    p.inputs['Metallic'].default_value = metal
    p.inputs['Roughness'].default_value = rough
    if emission:
        p.inputs['Emission Color'].default_value = (*color, 1)
        p.inputs['Emission Strength'].default_value = emission
    return m


M = {
    'ivory': material('M_PRF_CeramicIvory', (0.63, 0.69, 0.66), 0.42, 0.32),
    'orange': material('M_PRF_BurntOrange', (0.56, 0.105, 0.025), 0.38, 0.32),
    'dark': material('M_PRF_Graphite', (0.022, 0.044, 0.054), 0.65, 0.3),
    'black': material('M_PRF_Rubber', (0.012, 0.019, 0.022), 0.05, 0.6),
    'steel': material('M_PRF_BrushedSteel', (0.24, 0.33, 0.35), 0.85, 0.32),
    'brass': material('M_PRF_ChampagneMetal', (0.49, 0.30, 0.105), 0.8, 0.3),
    'cyan': material('M_PRF_StatusCyan', (0.015, 0.67, 0.88), 0.25, 0.25, 2.0),
    'amber': material('M_PRF_StatusAmber', (1.0, 0.28, 0.025), 0.2, 0.25, 1.5),
    'white': material('M_PRF_Lettering', (0.82, 0.88, 0.81), 0.1, 0.45),
}


def collection(name):
    global CURRENT
    CURRENT = bpy.data.collections.new(name)
    SCENE.collection.children.link(CURRENT)
    return CURRENT


def own(obj, name, mat=None):
    obj.name = name
    for c in list(obj.users_collection):
        c.objects.unlink(obj)
    CURRENT.objects.link(obj)
    if mat:
        obj.data.materials.append(M[mat])
    return obj


def bevel(obj, width=0.03, segments=3):
    if width:
        mod = obj.modifiers.new('Machined edge radii', 'BEVEL')
        mod.width = width
        mod.segments = segments
    mod = obj.modifiers.new('Weighted corner normals', 'WEIGHTED_NORMAL')
    mod.keep_sharp = True
    return obj


def box(name, loc, size, mat, edge=0.03, rot=None):
    bpy.ops.mesh.primitive_cube_add(size=1, location=loc)
    o = own(bpy.context.object, name, mat)
    o.dimensions = size
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    if rot:
        o.rotation_euler = rot
    return bevel(o, edge)


def cyl(name, loc, radius, depth, mat, axis='Z', vertices=32, edge=0.015):
    bpy.ops.mesh.primitive_cylinder_add(vertices=vertices, radius=radius, depth=depth, location=loc)
    o = own(bpy.context.object, name, mat)
    if axis == 'Y':
        o.rotation_euler.x = pi / 2
    elif axis == 'X':
        o.rotation_euler.y = pi / 2
    for p in o.data.polygons:
        p.use_smooth = len(p.vertices) == 4
    return bevel(o, edge, 2)


def ring(name, loc, radius, tube, mat, axis='Z', major_segments=40):
    bpy.ops.mesh.primitive_torus_add(major_radius=radius, minor_radius=tube,
                                   major_segments=major_segments, minor_segments=8, location=loc)
    o = own(bpy.context.object, name, mat)
    if axis == 'Y':
        o.rotation_euler.x = pi / 2
    elif axis == 'X':
        o.rotation_euler.y = pi / 2
    for p in o.data.polygons:
        p.use_smooth = True
    return o


def pipe(name, points, radius, mat):
    curve = bpy.data.curves.new(name, 'CURVE')
    curve.dimensions = '3D'
    curve.resolution_u = 1
    curve.bevel_depth = radius
    curve.bevel_resolution = 2
    curve.use_fill_caps = True
    spline = curve.splines.new('POLY')
    spline.points.add(len(points)-1)
    for p, co in zip(spline.points, points):
        p.co = (*co, 1)
    o = bpy.data.objects.new(name, curve)
    CURRENT.objects.link(o)
    o.data.materials.append(M[mat])
    return o


def text(name, label, loc, size, mat='white', rot=(pi/2, 0, 0)):
    curve = bpy.data.curves.new(name, 'FONT')
    curve.body = label
    curve.size = size
    curve.align_x = 'CENTER'
    curve.align_y = 'CENTER'
    curve.extrude = 0.0005
    curve.resolution_u = 3
    o = bpy.data.objects.new(name, curve)
    o.location = loc
    o.rotation_euler = rot
    CURRENT.objects.link(o)
    o.data.materials.append(M[mat])
    return o


def bolt(name, loc, axis='Y', r=0.028):
    cyl(name, loc, r, 0.018, 'steel', axis, vertices=6, edge=0.003)


def front_panel(name, loc, size, mat='ivory'):
    x,y,z = loc
    w,d,h = size
    box(name+'_gasket', (x,y+0.02,z), (w+0.045,d,h+0.045), 'black', 0.035)
    box(name, loc, size, mat, 0.035)
    for dx in [-w/2+0.08, w/2-0.08]:
        for dz in [-h/2+0.075, h/2-0.075]:
            bolt(name+'_fastener', (x+dx,y-d/2-0.009,z+dz), r=0.021)


def warning_band(name, loc, width, height):
    x,y,z=loc
    box(name+'_plate', loc, (width,0.018,height), 'brass', 0.005)
    # Individual vertical slashes remain inside plate bounds.
    for i in range(int(width/0.11)):
        box(name+'_stripe', (x-width/2+0.07+i*0.11,y-0.014,z),
            (0.041,0.009,height*0.8), 'dark', 0.002, (0,-0.3,0))


def feet(width, depth):
    for x in (-width/2+0.22,width/2-0.22):
        for y in (-depth/2+0.22,depth/2-0.22):
            box('Isolation_foot', (x,y,0.09), (0.42,0.42,0.18), 'black', 0.04)
            box('Foot_mount', (x,y,0.2), (0.32,0.32,0.11), 'steel', 0.025)


def hatch_glyph(x,y,z,scale=1):
    # Original geometric egg insignia, opaque enclosure; not an actual egg.
    points=[]
    for i in range(33):
        a=2*pi*i/32
        points.append((x+scale*0.12*sin(a)*(0.88-0.16*cos(a)),y,z+scale*0.18*cos(a)))
    pipe('Egg_insignia', points, scale*0.008, 'cyan')
    pipe('Egg_insignia_divider', [(x-0.067*scale,y,z),(x+0.067*scale,y,z)], 0.005*scale, 'cyan')


def hopper():
    # Thick open frustum with a sealed, recessed inner baffle. No exposed blades.
    verts=[]
    for w,d,z in ((1.04,.78,2.08),(1.66,1.30,2.75),(1.48,1.12,2.75),(.9,.65,2.16)):
        verts += [(x-0.25,y+0.03,z) for x,y in ((-w/2,-d/2),(w/2,-d/2),(w/2,d/2),(-w/2,d/2))]
    faces=[]
    for a,b in ((0,4),(4,8),(8,12),(12,0)):
        faces += [(a+i,a+(i+1)%4,b+(i+1)%4,b+i) for i in range(4)]
    mesh=bpy.data.meshes.new('Thick hopper walls')
    mesh.from_pydata(verts,[],faces)
    mesh.update()
    o=bpy.data.objects.new('Sealed_intake_hopper',mesh)
    CURRENT.objects.link(o)
    mesh.materials.append(M['orange'])
    bevel(o,.022,3)
    box('Hopper_internal_baffle',(-.25,.03,2.165),(.93,.68,.055),'black',.025)
    for y in (-.17,.08,.3):
        box('Intake_baffle_rib',(-.25,y,2.2),(.84,.045,.04),'steel',.012)
    for x in (-1.065,.565):
        box('Hopper_edge', (x,.03,2.75),(.055,1.31,.055),'steel',.013)
    for y in (-.62,.68):
        box('Hopper_edge',(-.25,y,2.75),(1.68,.055,.055),'steel',.013)
    text('Intake_label','EGG INLET',(-.25,-.634,2.645),.092)


def processor():
    c=collection('PRF_26_EggResourceProcessor')
    feet(2.7,2.04)
    box('Chassis_lower',(0,0,.34),(2.7,2.04,.26),'dark',.075)
    box('Chassis_trim',(0,-.03,.5),(2.55,1.92,.10),'brass',.028)
    box('Enclosed_processing_body',(-.25,.04,1.3),(1.94,1.69,1.5),'dark',.16)
    box('Shoulder_armor',(-.25,.06,1.98),(2.07,1.78,.23),'ivory',.07)
    front_panel('Front_armor',(-.25,-.832,1.29),(1.82,.10,1.31),'ivory')
    for x in (-1.23,.73):
        box('Structural_corner', (x,-.66,1.27),(.14,.23,1.25),'orange',.04)
    # Deep octagonal, permanently closed service hatch.
    cyl('Chamber_hatch_mount',(-.25,-.923,1.4),.53,.105,'dark','Y',8,.035)
    cyl('Chamber_hatch_bezel',(-.25,-.990,1.4),.46,.058,'brass','Y',8,.013)
    cyl('Chamber_hatch_sealed_face',(-.25,-1.027,1.4),.416,.04,'dark','Y',8,.012)
    ring('Hatch_status_ring',(-.25,-1.053,1.4),.347,.012,'cyan','Y',48)
    hatch_glyph(-.25,-1.069,1.435,1.15)
    text('Hatch_micro_label','SEALED',(-.25,-1.068,1.195),.059)
    for i in range(8):
        a=2*pi*i/8+pi/8
        bolt('Hatch_bolt',(-.25+.481*cos(a),-1.006,1.4+.481*sin(a)),r=.022)
    # Lower sealed materials transfer port, not a storage chest.
    box('Transfer_port_surround',(-.25,-.933,.733),(.78,.16,.28),'dark',.038)
    box('Transfer_port_closed',(-.25,-1.027,.733),(.56,.038,.13),'steel',.022)
    box('Transfer_port_status',(-.25,-1.05,.733),(.34,.012,.019),'cyan',.005)
    warning_band('Transfer_warning',(-.25,-1.011,.914),.67,.06)
    # Side drive: motor axis +X with cast ribs and vented end cap.
    box('Motor_cradle',(1.08,.10,.75),(.49,.93,.38),'orange',.06)
    cyl('Drive_motor',(1.08,.10,1.22),.40,.59,'dark','X',32,.03)
    for x in (.87,.96,1.05,1.14,1.23):
        cyl('Motor_cooling_fin',(x,.10,1.22),.438,.042,'steel','X',32,.008)
    cyl('Motor_end_cap',(1.39,.10,1.22),.39,.11,'orange','X',32,.022)
    cyl('Motor_vent_inset',(1.454,.10,1.22),.282,.016,'black','X',24,.008)
    for zoff in (-.16,-.08,0,.08,.16):
        length=2*(max(.23**2-zoff**2,0)**.5)
        box('Motor_end_grille',(1.468,.10,1.22+zoff),(.019,length,.025),'steel',.006)
    box('Motor_junction',(1.11,.1,1.72),(.38,.4,.18),'orange',.04)
    pipe('Drive_cable',[(1.11,.21,1.8),(1.11,.6,1.92),(.77,.72,1.92),(.56,.72,1.72)],.037,'black')
    pipe('Cooling_supply',[(.75,-.13,.73),(1.15,-.23,.7),(1.39,-.23,.83),(1.39,-.1,.94)],.035,'brass')
    # Left side service grille and rear exhaust.
    box('Left_service_panel',(-1.25,.05,1.27),(.09,1.13,.81),'orange',.045)
    for z in (.98,1.10,1.22,1.34,1.46,1.58):
        box('Left_cooling_louvre',(-1.304,.05,z),(.04,.81,.047),'black',.009)
    cyl('Rear_exhaust_stack',(-.87,.72,2.24),.115,.54,'dark','Z',24,.012)
    cyl('Rear_exhaust_cap',(-.87,.72,2.53),.16,.07,'steel','Z',24,.012)
    for x in (-.88,.35):
        box('Rear_service_armor',(x,.92,1.30),(.53,.09,1.03),'ivory',.04)
    box('Rear_power_socket',(-.22,.922,.80),(.31,.15,.3),'black',.033)
    cyl('Rear_power_connector',(-.22,1.007,.80),.081,.042,'brass','Y',12,.008)
    # Accessible display set to READY, no fake progress bars or moving fans.
    box('Controller_mount',(.76,-1.005,1.88),(.47,.22,.39),'dark',.04)
    box('Controller_screen',(.76,-1.127,1.90),(.34,.018,.19),'black',.018)
    text('Controller_text','READY',(.76,-1.141,1.92),.052,'cyan')
    for x in (.65,.76,.87):
        cyl('Controller_key',(x,-1.143,1.784),.02,.018,'brass','Y',12,.003)
    text('Nameplate','PRF / 26',(-.98,-.9,1.74),.069,'dark')
    box('Front_status_bar',(-.93,-.902,1.42),(.032,.022,.35),'cyan',.006)
    hopper()
    c['description']='26-tier sealed egg processor. Visual asset only; no gameplay.'
    return c


def breeding_pod(x, label):
    cyl('Pod_foot',(x,.12,.63),.72,.22,'steel','Z',12,.045)
    cyl('Pod_dark_core',(x,.12,1.62),.64,1.92,'dark','Z',32,.07)
    cyl('Pod_lower_armor',(x,.12,.91),.70,.40,'ivory','Z',12,.047)
    cyl('Pod_upper_armor',(x,.12,2.45),.73,.28,'ivory','Z',12,.043)
    cyl('Pod_crown',(x,.12,2.66),.62,.16,'dark','Z',12,.023)
    cyl('Pod_crown_inlay',(x,.12,2.76),.46,.09,'brass','Z',12,.021)
    cyl('Pod_crown_cap',(x,.12,2.82),.32,.075,'ivory','Z',12,.024)
    ring('Pod_upper_status',(x,.12,2.32),.65,.024,'cyan')
    ring('Pod_lower_status',(x,.12,1.12),.65,.017,'cyan')
    # Faceted opaque door; the centre never shows a Pal or a hatch preview.
    front_panel('Parent_pod_sealed_door',(x,-.503,1.73),(.82,.11,1.13),'ivory')
    box('Parent_pod_face_inset',(x,-.574,1.78),(.59,.035,.79),'dark',.075)
    for dx in (-.355,.355):
        box('Parent_pod_light_edge',(x+dx,-.576,1.77),(.026,.024,.74),'cyan',.008)
    cyl('Parent_pod_medallion',(x,-.610,1.89),.205,.055,'brass','Y',12,.012)
    cyl('Parent_pod_medallion_face',(x,-.644,1.89),.17,.025,'dark','Y',12,.008)
    text('Parent_ID',label,(x,-.662,1.90),.215)
    text('Pod_label','PARENT / '+label,(x,-.6,1.50),.061)
    box('Parent_pod_lower_badge',(x,-.577,1.32),(.30,.025,.064),'brass',.008)
    for dx in (-.50,.50):
        box('Pod_side_spine',(x+dx,-.225,1.71),(.13,.31,1.3),'ivory',.045)
        box('Pod_side_spine_trim',(x+dx,-.39,1.74),(.047,.02,.85),'brass',.008)
    for i in range(6):
        box('Pod_rear_louvre',(x,.763,1.35+i*.145),(.53,.058,.052),'steel',.012)
    for dx in (-.31,.31):
        bolt('Pod_crown_bolt',(x+dx,-.26,2.64),axis='Z',r=.024)


def breeder():
    c=collection('PRF_36_ResourceBreedingFacility')
    feet(3.65,2.4)
    box('Shared_chassis',(0,.03,.34),(3.65,2.4,.27),'dark',.105)
    box('Chassis_perimeter',(0,.03,.505),(3.55,2.31,.12),'brass',.035)
    box('Upper_deck',(0,.06,.60),(3.41,2.15,.10),'ivory',.045)
    for x in (-1.36,1.36):
        front_panel('Front_lower_armor',(x,-1.143,.47),(.60,.07,.19),'ivory')
    # Closed common processing spine between the paired parent chambers.
    box('Central_processing_spine',(0,.51,1.67),(.65,.86,2.03),'dark',.11)
    box('Central_dorsal_armor',(0,.61,2.62),(.8,.86,.22),'ivory',.06)
    breeding_pod(-.92,'A')
    breeding_pod(.92,'B')
    # Symmetrical bridge joins both pods to one opaque centre.
    box('Bridge_beam',(0,.64,2.98),(2.42,.46,.24),'dark',.068)
    box('Bridge_upper_enamel',(0,.64,3.115),(2.54,.53,.10),'ivory',.035)
    box('Bridge_cyan_inlay',(0,.396,2.983),(1.65,.021,.044),'cyan',.012)
    for x in (-1.03,1.03):
        box('Bridge_pylon',(x,.67,2.80),(.23,.36,.48),'steel',.045)
        for z in (2.90,3.01):
            bolt('Bridge_fastener',(x,.421,z),r=.021)
    for x in (-.41,.41):
        pipe('Coolant_return',[(x,.79,1.0),(x,.99,1.15),(x,.99,2.65),(x,.79,2.91)],.051,'brass')
        for z in (1.37,1.60,1.83,2.06,2.29):
            cyl('Pipe_collars',(x,.99,z),.072,.048,'dark','Z',16,.008)
    # Central user console and single cake insertion hatch.
    box('Console_column',(0,-.61,1.18),(.70,.68,1.1),'dark',.09)
    front_panel('Cake_input_surround',(0,-1.0,.88),(.74,.13,.39),'ivory')
    box('Cake_input_seal',(0,-1.085,.895),(.50,.044,.17),'black',.025)
    box('Cake_input_closed_flap',(0,-1.114,.895),(.41,.023,.095),'brass',.015)
    text('Cake_input_label','CAKE',(0,-1.08,1.025),.063,'dark')
    box('Console_head',(0,-.75,1.61),(.69,.40,.43),'ivory',.06)
    box('Console_screen',(0,-.966,1.64),(.49,.022,.24),'black',.025)
    text('Console_title','PRF / 36',(0,-.983,1.691),.063,'cyan')
    text('Console_mode','LINK READY',(0,-.983,1.593),.046)
    for x in (-.2,0,.2):
        cyl('Console_key',(x,-.984,1.455),.028,.024,'brass','Y',16,.007)
    box('Core_top_inspection',(0,-.065,2.255),(.39,.09,.36),'steel',.04)
    hatch_glyph(0,-.116,2.258,.68)
    warning_band('Core_warning',(0,-.145,2.03),.40,.055)
    # Lateral coolant unit with armored receiver, linked to the spine.
    cyl('Cryo_receiver',(1.51,.63,1.17),.23,1.04,'steel','Z',24,.035)
    cyl('Cryo_receiver_base',(1.51,.63,.65),.27,.14,'dark','Z',12,.014)
    cyl('Cryo_receiver_cap',(1.51,.63,1.72),.27,.12,'brass','Z',12,.019)
    for z in (.87,1.03,1.19,1.35,1.51):
        ring('Cryo_receiver_rib',(1.51,.63,z),.232,.021,'dark',major_segments=24)
    box('Cryo_status_mount',(1.51,.394,1.20),(.18,.05,.59),'dark',.022)
    box('Cryo_status_lens',(1.51,.361,1.20),(.046,.019,.42),'cyan',.007)
    pipe('Cryo_supply',[(1.51,.63,1.78),(1.51,.63,2.06),(1.30,.81,2.17),(.99,.69,2.17)],.042,'steel')
    # Front transfer connection - no inventory storage drawer.
    for x in (-.62,.62):
        cyl('Transfer_connector',(x,-1.158,.435),.085,.06,'steel','Y',12,.009)
        cyl('Transfer_connector_cover',(x,-1.195,.435),.052,.016,'dark','Y',12,.005)
    text('Chassis_name','RESOURCE / LINK',(0,-1.186,.433),.055)
    # Rear service enclosure and cable connection complete all viewing sides.
    front_panel('Rear_cooling_panel',(0,1.13,1.59),(1.02,.10,1.17),'dark')
    for z in (1.21,1.36,1.51,1.66,1.81,1.96):
        box('Rear_heat_exchanger',(0,1.20,z),(.78,.09,.06),'steel',.012)
    box('Rear_power_block',(-1.38,.96,.9),(.35,.32,.4),'orange',.04)
    pipe('Rear_power_cable',[(-1.38,1.12,1.09),(-1.38,1.24,1.34),(-.68,1.24,1.42),(-.44,1.12,1.42)],.037,'black')
    c['description']='36-tier paired sealed parent pods; no external egg inlet or exposed conveyor. Visual only.'
    return c


def freeze_meshes(col):
    """Evaluate bevels/curves into portable geometry; make one UV layer per piece."""
    for o in list(col.objects):
        bpy.ops.object.select_all(action='DESELECT')
        o.select_set(True)
        bpy.context.view_layer.objects.active=o
        bpy.ops.object.convert(target='MESH')
        bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)
        if not o.data.uv_layers:
            bpy.ops.object.mode_set(mode='EDIT')
            bpy.ops.mesh.select_all(action='SELECT')
            bpy.ops.uv.smart_project(angle_limit=radians(66), island_margin=.02)
            bpy.ops.object.mode_set(mode='OBJECT')
    bpy.ops.object.select_all(action='DESELECT')


def mesh_bounds(objs):
    points=[o.matrix_world @ Vector(c) for o in objs for c in o.bound_box]
    lo=[min(v[i] for v in points) for i in range(3)]
    hi=[max(v[i] for v in points) for i in range(3)]
    return {'min_m':lo,'max_m':hi,'size_m':[hi[i]-lo[i] for i in range(3)]}


ASSETS=[processor(),breeder()]
report={}
for col in ASSETS:
    print('Preparing asset:',col.name,flush=True)
    freeze_meshes(col)
    objs=list(col.objects)
    bounds=mesh_bounds(objs)
    triangles=0
    for o in objs:
        o.data.calc_loop_triangles()
        triangles+=len(o.data.loop_triangles)
    report[col.name]={'parts':len(objs),'triangles':triangles,'bounds':bounds,
                      'uv0_on_all_meshes':all(bool(o.data.uv_layers) for o in objs),
                      'front':'-Y','up':'+Z','units':'metres'}
    # Export consolidated base mesh, plus separate status lights. Originals remain editable.
    export_col=collection(col.name+'_EXPORT')
    copies=[]
    for o in objs:
        dupe=o.copy()
        dupe.data=o.data.copy()
        export_col.objects.link(dupe)
        copies.append(dupe)
    root_name='SM_PRF_EggResourceProcessor' if '_26_' in col.name else 'SM_PRF_ResourceBreedingFacility'
    groups={flag:[o for o in copies if any('Status' in m.name for m in o.data.materials)==flag]
            for flag in (False,True)}
    for emissive,suffix in ((False,''),(True,'_Status')):
        group=groups[emissive]
        bpy.ops.object.select_all(action='DESELECT')
        for o in group:
            o.select_set(True)
        bpy.context.view_layer.objects.active=group[0]
        bpy.ops.object.join()
        joined=bpy.context.object
        joined.name=root_name+suffix
        SCENE.cursor.location=(0,0,0)
        bpy.ops.object.origin_set(type='ORIGIN_CURSOR')
    # Simple convex pieces instead of detailed automatic hulls; named to UE convention.
    if '_26_' in col.name:
        hulls=[((-.25,-.025,1.12),(2.04,2.20,2.08)),((-.25,.03,2.41),(1.7,1.36,.7)),
               ((1.1,.10,1.2),(.75,.9,1.14)),((0,0,.24),(2.7,2.04,.48))]
    else:
        hulls=[((0,.03,.33),(3.65,2.4,.66)),((-.92,.12,1.75),(1.46,1.46,2.2)),
               ((.92,.12,1.75),(1.46,1.46,2.2)),((0,.62,2.96),(2.55,.6,.41)),
               ((0,-.59,1.3),(.78,1.22,1.27)),((1.51,.63,1.18),(.55,.55,1.25)),
               ((0,.55,1.72),(.86,1.43,2.0))]
    for i,(loc,size) in enumerate(hulls):
        o=box(f'UCX_{root_name}_{i:02d}',loc,size,'dark',0)
        # Remove normal modifier on collision boxes.
        o.modifiers.clear()
        o.display_type='WIRE'
        o.hide_render=True
    bpy.ops.object.select_all(action='DESELECT')
    for o in export_col.objects:
        o.select_set(True)
    bpy.ops.export_scene.fbx(filepath=str(OUT/'models'/f'{root_name}.fbx'),use_selection=True,
        object_types={'MESH'},global_scale=1.0,apply_unit_scale=True,apply_scale_options='FBX_SCALE_UNITS',
        axis_forward='-Y',axis_up='Z',use_mesh_modifiers=True,mesh_smooth_type='FACE',
        add_leaf_bones=False,bake_anim=False,path_mode='AUTO')
    export_col.hide_render=True
    export_col.hide_viewport=True
    export_col.hide_select=True
    # Include a scene so opening the individual .blend actually displays the model.
    asset_scene=bpy.data.scenes.new(root_name)
    asset_scene.unit_settings.system='METRIC'
    asset_scene.unit_settings.scale_length=1.0
    asset_scene.collection.children.link(col)
    asset_scene.collection.children.link(export_col)
    bpy.data.libraries.write(str(OUT/'models'/f'{root_name}.blend'),{asset_scene},fake_user=True)
    bpy.data.scenes.remove(asset_scene)


# Studio exists only in the master file and previews, never in game exports.
studio=collection('STUDIO_NOT_FOR_EXPORT')
M['floor']=material('Studio_Backdrop',(.055,.078,.091),.05,.56)
box('Studio_ground',(0,0,-.13),(200,200,.25),'floor',0)


def area(name,location,energy,color,size,target=(0,0,1.3)):
    data=bpy.data.lights.new(name,'AREA')
    data.energy=energy
    data.color=color
    data.shape='DISK'
    data.size=size
    ob=bpy.data.objects.new(name,data)
    studio.objects.link(ob)
    ob.location=location
    ob.rotation_euler=(Vector(target)-ob.location).to_track_quat('-Z','Y').to_euler()


area('Key_softbox',(1,-5,7),1700,(.85,.95,1),5)
area('Warm_fill',(-5,-1,4),1200,(1,.72,.48),4)
area('Rear_edge',(2,5,6),2100,(.45,.78,1),3)
data=bpy.data.cameras.new('Presentation_camera')
camera=bpy.data.objects.new('Presentation_camera',data)
studio.objects.link(camera)
SCENE.camera=camera
data.type='ORTHO'
data.lens=55


def camera_to(loc,target,scale):
    camera.location=loc
    camera.rotation_euler=(Vector(target)-camera.location).to_track_quat('-Z','Y').to_euler()
    camera.data.ortho_scale=scale


def render(filename,cols,loc,target,scale,res=(1440,1200)):
    for col in ASSETS:
        col.hide_render=col not in cols
    camera_to(loc,target,scale)
    SCENE.render.resolution_x,SCENE.render.resolution_y=res
    SCENE.render.filepath=str(OUT/'previews'/filename)
    bpy.ops.render.render(write_still=True)
    print('RENDER_OK',filename,flush=True)


(OUT/'asset_report.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
render('processor_hero.png',[ASSETS[0]],(6,-8,5.4),(0,0,1.32),4.5)
render('breeder_hero.png',[ASSETS[1]],(6.5,-9,6),(0,.04,1.48),5.45)
render('processor_rear.png',[ASSETS[0]],(-6,8,5),(0,0,1.3),4.5,(1100,1000))
render('breeder_rear.png',[ASSETS[1]],(-7,9,5.9),(0,.05,1.46),5.45,(1100,1000))

# One comparative view, same scale, grounded and spaced. This is the master default view.
for o in ASSETS[0].objects:
    o.location.x-=2.12
for o in ASSETS[1].objects:
    o.location.x+=1.60
render('facilities_lineup.png',ASSETS,(7,-13,7),(0,.02,1.38),9.1,(1800,1050))
for col in ASSETS:
    col.hide_viewport=False
    col.hide_render=False
bpy.ops.object.select_all(action='DESELECT')
for screen in bpy.data.screens:
    for a in screen.areas:
        if a.type=='VIEW_3D':
            a.spaces.active.region_3d.view_perspective='CAMERA'
            a.spaces.active.shading.type='MATERIAL'
SCENE['PRF_README']='Original PRF artwork. Main scene is presentation. models/*.blend and FBX have bottom-centre origins. Assets only, NOT a functional or game-tested MOD.'
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'PalResourceFactory_Facilities.blend'))
print('PRF_ART_COMPLETE',json.dumps(report),flush=True)
