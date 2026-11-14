# Builds /Game/Leafline/Materials/M_TreeBark3 as a DUPLICATE of M_TreeBark2 (its cutaway, buried test, shadow pass and
# opacity chain stay exactly as they are) and adds a new Custom node "Bark3" (bark3_bark.hlsl) fed by fresh source nodes:
# UV0..UV3, vertex colour RGB and its A output, world position, vertex normal and tangent, MPC_View CamPos. Base colour,
# roughness, normal and AO are reconnected to Bark3; the old "Bark" node stays in the graph, unused (the M_Ant2 pattern:
# a fresh node is grown, never the linked one). The C++ side (leaf.SkirtBlend, set by Leaf.Bark 2) writes the seam
# cross-fade data into UV0, UV3.y, colour R and alpha; see the header of bark3_bark.hlsl.
# It is the body of an unreal-mcp ProgrammaticToolset.execute_tool_script call (the main session pastes it; agents never
# write .uasset files). dup() uses AssetTools.duplicate; with DUP = False the script expects M_TreeBark3 to exist already.
# MODE='code' only replaces the Bark3 node's Code on an existing M_TreeBark3 (after editing the HLSL, e.g. its tuning
# constants or DEBUG_VIEW): never delete and recreate an asset of the same name in one editor session (CLAUDE.md).
import json
MODE = 'build'  # 'build' duplicates M_TreeBark2 and adds the Bark3 node; 'code' updates Bark3's Code and recompiles
DUP = True
HLSL = 'D:/UnrealProjects/AntTreeGame/Tools/materials/bark3_bark.hlsl'  # first built from the seams worktree (14 Nov night)
SRC = '/Game/Leafline/Materials/M_TreeBark2.M_TreeBark2'
MAT = '/Game/Leafline/Materials/M_TreeBark3.M_TreeBark3'
MPC = '/Game/Leafline/Materials/MPC_View.MPC_View'
M = 'editor_toolset.toolsets.material.MaterialTools.'
O = 'editor_toolset.toolsets.object.ObjectTools.'
A = 'editor_toolset.toolsets.asset.AssetTools.'

def t(name, args):
    return execute_tool(name, json.dumps(args))

def rp(e):
    return e['refPath'] if isinstance(e, dict) else str(e)

def code():
    with open(HLSL, 'r') as f:
        return f.read()

def sp(path, vals):
    return t(O + 'set_properties', {'instance': {'refPath': path}, 'values': json.dumps(vals)})['returnValue']

def gp(path, prop):
    got = t(O + 'get_properties', {'instance': {'refPath': path}, 'properties': [prop]})['returnValue']
    return (json.loads(got) if isinstance(got, str) else got)[prop]

def add(cls, x, y, props=None):
    e = rp(t(M + 'add_expression', {'material_or_function': {'refPath': MAT}, 'expression_class': {'refPath': '/Script/Engine.MaterialExpression' + cls}, 'x': x, 'y': y})['returnValue'])
    if props:
        sp(e, props)
    return e

def key(d, base):
    for k in (base[0].lower() + base[1:], base):
        if k in d:
            return k
    return base

def grow(path, prop, items):
    # As build_bark2_mcp.py: struct arrays only take when the existing elements go back exactly as read and one element
    # is appended per call. Existing inputs keep their names and links; new ones are linked below.
    cur = gp(path, prop)
    changed = False
    for i in range(min(len(cur), len(items))):
        for kk, vv in items[i].items():
            k = key(cur[i], kk)
            if cur[i].get(k) != vv:
                cur[i][k] = vv
                changed = True
    if changed:
        sp(path, {prop: cur})
        cur = gp(path, prop)
    while len(cur) < len(items):
        it = items[len(cur)]
        el = dict(cur[-1]) if cur else dict(it)
        for kk, vv in it.items():
            el[key(el, kk)] = vv
        sp(path, {prop: cur + [el]})
        cur = gp(path, prop)

def con(a, ao, b, bi):
    t(M + 'connect_expressions', {'from_expression': {'refPath': a}, 'from_output_name': ao, 'to_expression': {'refPath': b}, 'to_input_name': bi})

def dup():
    # AssetTools.duplicate (the main session, 14 Nov: there is no duplicate_asset); returns True.
    return t(A + 'duplicate', {'path': '/Game/Leafline/Materials/M_TreeBark2', 'new_path': '/Game/Leafline/Materials/M_TreeBark3'})

def custom(x, y, desc, inputs, outtype, extra=None):
    e = add('Custom', x, y, {'Code': code(), 'OutputType': outtype, 'Description': desc})
    grow(e, 'Inputs', [{'InputName': n} for n in inputs])
    if extra:
        grow(e, 'AdditionalOutputs', [{'OutputName': n, 'OutputType': ty} for (n, ty) in extra])
    return e

def custom_node(desc):
    exprs = [rp(e) for e in t(M + 'get_expressions', {'material_or_function': {'refPath': MAT}})['returnValue']]
    for e in exprs:
        if 'MaterialExpressionCustom' not in e:
            continue
        got = t(O + 'get_properties', {'instance': {'refPath': e}, 'properties': ['Description']})['returnValue']
        if (json.loads(got) if isinstance(got, str) else got).get('Description') == desc:
            return e
    raise Exception('no Custom node "%s" in %s' % (desc, MAT))

def out(e, name, prop):
    t(M + 'connect_to_output', {'expression': {'refPath': e}, 'output_name': name, 'material_property': prop})

def build():
    if DUP:
        dup()
    X = -2600
    uv = [add('TextureCoordinate', X, -1400 + 120 * i, {'CoordinateIndex': i}) for i in range(4)]
    vc = add('VertexColor', X, -920)
    wp = add('WorldPosition', X, -800)
    nrm = add('VertexNormalWS', X, -680)
    tan = add('VertexTangentWS', X, -560)
    cam = add('CollectionParameter', X, -440, {'Collection': MPC, 'ParameterName': 'CamPos'})
    bark = custom(-2000, -1000, 'Bark3', ['UV0', 'UV1', 'UV2', 'UV3', 'VC', 'VA', 'WP', 'N', 'T', 'Cam'], 'CMOT_Float3',
                  [('Rough', 'CMOT_Float1'), ('NormalW', 'CMOT_Float3'), ('AO', 'CMOT_Float1')])
    for i in range(4):
        con(uv[i], '', bark, 'UV%d' % i)
    # The vertex colour's main output is RGB only; alpha (1 - the seam weight) comes from its A output.
    con(vc, '', bark, 'VC')
    con(vc, 'A', bark, 'VA')
    for (src, pin) in [(wp, 'WP'), (nrm, 'N'), (tan, 'T'), (cam, 'Cam')]:
        con(src, '', bark, pin)
    # Connecting a new expression to a material output replaces the old link (M_Ant2). The opacity mask is not touched.
    out(bark, '', 'MP_BaseColor')
    out(bark, 'Rough', 'MP_Roughness')
    out(bark, 'NormalW', 'MP_Normal')
    out(bark, 'AO', 'MP_AmbientOcclusion')
    ins = [i.get('InputName', i.get('inputName')) for i in gp(bark, 'Inputs')]
    outs = t(M + 'get_expression_output_names', {'expression': {'refPath': bark}})['returnValue']
    return {'bark3': bark, 'inputs': ins, 'outputs': outs}

def update_code():
    bark = custom_node('Bark3')
    sp(bark, {'Code': code()})
    # Setting only Code did not always mark the package dirty (13 Nov): nudge a material property so it saves.
    sp(MAT, {'OpacityMaskClipValue': 0.5001})
    sp(MAT, {'OpacityMaskClipValue': 0.5})
    return {'updated': bark}

def run():
    res = build() if MODE == 'build' else update_code()
    errs = []
    try:
        t(M + 'recompile', {'material_or_function': {'refPath': MAT}})
    except Exception as ex:
        errs.append(str(ex)[:1500])
    t(A + 'save_assets', {'asset_paths': []})
    res['errors'] = errs
    return res
