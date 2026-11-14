# Builds /Game/Leafline/Materials/M_Litter: a matte plain-colour material for the forest floor (AForestFloor). Opaque, default
# lit, one-sided (the floor emits both windings where a leaf can be seen from below), Roughness 1, Specular 0, Base Colour
# from a vector parameter 'Color' (the same name as BasicShapeMaterial's, so the dynamic instances set it the same way).
# BasicShapeMaterial is glossy and reflects the sky at grazing angles (shadowed flat leaves read grey-violet).
# It is the body of an unreal-mcp ProgrammaticToolset.execute_tool_script call (the main session pastes it; agents never
# write .uasset files). Run once: never delete and recreate an asset of the same name in one editor session (CLAUDE.md).
import json
MAT = '/Game/Leafline/Materials/M_Litter.M_Litter'
M = 'editor_toolset.toolsets.material.MaterialTools.'
O = 'editor_toolset.toolsets.object.ObjectTools.'
A = 'editor_toolset.toolsets.asset.AssetTools.'

def t(name, args):
    return execute_tool(name, json.dumps(args))

def rp(e):
    return e['refPath'] if isinstance(e, dict) else str(e)

def node_code(n):
    with open(HLSL, 'r') as f:
        src = f.read()
    b = src.index('---- BEGIN NODE %d' % n)
    b = src.index('\n', b) + 1
    e = src.index('// ---- END NODE %d' % n, b)
    return src[b:e]

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
    # Same as build_bark2_mcp.py: struct arrays only take when existing elements go back exactly as read, one per call.
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

def custom(x, y, desc, n, inputs, outtype, extra=None):
    e = add('Custom', x, y, {'Code': node_code(n), 'OutputType': outtype, 'Description': desc})
    grow(e, 'Inputs', [{'InputName': i} for i in inputs])
    if extra:
        grow(e, 'AdditionalOutputs', [{'OutputName': nm, 'OutputType': ty} for (nm, ty) in extra])
    return e

def con(a, ao, b, bi):
    t(M + 'connect_expressions', {'from_expression': {'refPath': a}, 'from_output_name': ao, 'to_expression': {'refPath': b}, 'to_input_name': bi})

def out(e, name, prop):
    t(M + 'connect_to_output', {'expression': {'refPath': e}, 'output_name': name, 'material_property': prop})


def set_color(e, r, g, b):
    # The struct property's spelling differs between tool versions: try them in turn, ignore a default that will not take
    # (the dynamic instances set Color anyway).
    for v in ({'r': r, 'g': g, 'b': b, 'a': 1.0}, {'R': r, 'G': g, 'B': b, 'A': 1.0}, '(R=%s,G=%s,B=%s,A=1.0)' % (r, g, b)):
        try:
            sp(e, {'DefaultValue': v})
            return True
        except Exception:
            pass
    return False

def run():
    t(M + 'create_material', {'folder_path': '/Game/Leafline/Materials', 'asset_name': 'M_Litter'})
    col = add('VectorParameter', -600, -100, {'ParameterName': 'Color'})
    ok = set_color(col, 0.05, 0.03, 0.015)
    rough = add('Constant', -600, 100, {'R': 1.0})
    spec = add('Constant', -600, 200, {'R': 0.0})
    out(col, '', 'MP_BaseColor')
    out(rough, '', 'MP_Roughness')
    out(spec, '', 'MP_Specular')
    errs = []
    try:
        t(M + 'recompile', {'material_or_function': {'refPath': MAT}})
    except Exception as ex:
        errs.append(str(ex)[:1500])
    t(A + 'save_assets', {'asset_paths': []})
    return {'color_default_set': ok, 'errors': errs}
