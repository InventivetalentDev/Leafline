# Adds the cutaway tuning scalars to MPC_View (look pass, 13 Nov): body of an unreal-mcp execute_tool_script call.
# AAntCamera::PublishView sets them every frame from FAntCameraTuning.
import json
MPC = '/Game/Leafline/Materials/MPC_View.MPC_View'
O = 'editor_toolset.toolsets.object.ObjectTools.'
A = 'editor_toolset.toolsets.asset.AssetTools.'
NEW = [('CutawayBand', 0.5), ('CutawayTaper', 1.0), ('NearFadeStart', 8.0), ('NearFadeEnd', 35.0)]

def t(name, args):
    return execute_tool(name, json.dumps(args))

def get():
    got = t(O + 'get_properties', {'instance': {'refPath': MPC}, 'properties': ['ScalarParameters']})['returnValue']
    return (json.loads(got) if isinstance(got, str) else got)['ScalarParameters']

def run():
    cur = get()
    names = [p.get('parameterName', p.get('ParameterName')) for p in cur]
    for (n, v) in NEW:
        if n in names:
            continue
        cur = cur + [{'ParameterName': n, 'DefaultValue': v}]
        t(O + 'set_properties', {'instance': {'refPath': MPC}, 'values': json.dumps({'ScalarParameters': cur})})
        cur = get()
    t(A + 'save_assets', {'asset_paths': []})
    after = get()
    return {'params': [(p.get('parameterName'), p.get('defaultValue'), p.get('id')) for p in after]}
