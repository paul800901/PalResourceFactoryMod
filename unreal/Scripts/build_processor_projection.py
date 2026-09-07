"""Attach a language-neutral material billboard to the existing PRF Blueprint."""
from pathlib import Path
import unreal

root = "/Game/PalResourceFactory/Processor"
lib = unreal.EditorAssetLibrary
edit = unreal.MaterialEditingLibrary
path = root + "/Materials/M_PRF_Projection"
mat = lib.load_asset(path) if lib.does_asset_exist(path) else None
if not mat:
    mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "M_PRF_Projection", root + "/Materials", unreal.Material, unreal.MaterialFactoryNew())
edit.delete_all_material_expressions(mat)
mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
mat.set_editor_property("two_sided", True)
uv = edit.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate)
clock = edit.create_material_expression(mat, unreal.MaterialExpressionTime)
data = edit.create_material_expression(mat, unreal.MaterialExpressionVectorParameter)
data.set_editor_property("parameter_name", "WorkCounts")
data.set_editor_property("default_value", unreal.LinearColor(0, 0, 0, 0))
data.set_editor_property("use_custom_primitive_data", True)
data.set_editor_property("primitive_data_index", 4)
node = edit.create_material_expression(mat, unreal.MaterialExpressionCustom)
node.set_editor_property("code", Path(__file__).with_name("processor_projection.hlsl").read_text())
node.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT4)
inputs=[]
for name in ("UV", "T", "Data"):
    inp=unreal.CustomInput()
    inp.set_editor_property("input_name", name)
    inputs.append(inp)
node.set_editor_property("inputs", inputs)
for source, target in ((uv,"UV"),(clock,"T"),(data,"Data")):
    if not edit.connect_material_expressions(source,"",node,target):
        raise RuntimeError("Projection input connection failed")
alpha=edit.create_material_expression(mat,unreal.MaterialExpressionComponentMask)
alpha.set_editor_property("r",False)
alpha.set_editor_property("g",False)
alpha.set_editor_property("b",False)
alpha.set_editor_property("a",True)
edit.connect_material_expressions(node,"",alpha,"")
edit.connect_material_property(node,"",unreal.MaterialProperty.MP_EMISSIVE_COLOR)
edit.connect_material_property(alpha,"",unreal.MaterialProperty.MP_OPACITY_MASK)
edit.recompile_material(mat)
lib.save_loaded_asset(mat)
bp=lib.load_asset(root+"/BP_PRF_EggProcessor")
ss=unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
helper=unreal.SubobjectDataBlueprintFunctionLibrary
entries=ss.k2_gather_subobject_data_for_blueprint(bp)
parent=next(h for h in entries if helper.get_object(helper.get_data(h)).get_name()=="PRF_Root_GEN_VARIABLE")
comp=None
for h in entries:
    obj=helper.get_object(helper.get_data(h))
    if obj.get_name()=="PRF_Projection_GEN_VARIABLE": comp=obj
if comp is None:
    h,reason=ss.add_new_subobject(unreal.AddNewSubobjectParams(parent_handle=parent,new_class=unreal.MaterialBillboardComponent,blueprint_context=bp))
    if str(reason): raise RuntimeError(str(reason))
    ss.rename_subobject(h,unreal.Text("PRF_Projection"))
    comp=helper.get_object(helper.get_data(h))
comp.set_editor_property("relative_location",unreal.Vector(0,0,375))
comp.set_editor_property("cast_shadow",False)
comp.set_collision_profile_name("NoCollision")
comp.set_editor_property("elements",[])
# UE's billboard vertices apply BaseSizeX vertically and BaseSizeY horizontally.
# Match the shader's float2(4,2) domain: world width 240, height 120.
comp.add_element(mat,None,False,60,120,None)
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
lib.save_loaded_asset(bp,only_if_is_dirty=False)
unreal.log("[PRF Projection] Saved camera-facing activity icons; aspect=2:1; CPD index=4; game-unverified")
