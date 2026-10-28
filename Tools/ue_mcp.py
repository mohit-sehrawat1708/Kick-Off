"""Tiny JSON-RPC client for the Unreal Editor MCP server (http://127.0.0.1:8000/mcp).

Usage:
  python ue_mcp.py list_toolsets
  python ue_mcp.py describe <ToolsetName>
  python ue_mcp.py call <ToolsetName> <tool_name> '<json arguments>'
  python ue_mcp.py script <file.py>   (runs via ProgrammaticToolset.execute_tool_script)
  python ue_mcp.py raw <method> '<json params>'
"""
import json
import sys
import urllib.request

URL = "http://127.0.0.1:8000/mcp"
HEADERS = {"Content-Type": "application/json", "Accept": "application/json, text/event-stream"}


def post(payload, session=None):
    headers = dict(HEADERS)
    if session:
        headers["Mcp-Session-Id"] = session
    req = urllib.request.Request(URL, data=json.dumps(payload).encode(), headers=headers)
    with urllib.request.urlopen(req, timeout=600) as resp:
        sid = resp.headers.get("Mcp-Session-Id") or session
        body = resp.read().decode("utf-8", "replace")
    if body.lstrip().startswith("data:") or "\ndata:" in body:
        lines = [l[5:].strip() for l in body.splitlines() if l.startswith("data:")]
        body = lines[-1] if lines else "{}"
    return sid, (json.loads(body) if body.strip() else {})


def session():
    sid, _ = post({"jsonrpc": "2.0", "id": 1, "method": "initialize", "params": {
        "protocolVersion": "2025-03-26", "capabilities": {}, "clientInfo": {"name": "claude-code", "version": "1"}}})
    post({"jsonrpc": "2.0", "method": "notifications/initialized"}, sid)
    return sid


def rpc(method, params):
    sid = session()
    _, res = post({"jsonrpc": "2.0", "id": 2, "method": method, "params": params}, sid)
    return res


def show(res):
    result = res.get("result", res)
    if isinstance(result, dict) and "content" in result:
        for item in result["content"]:
            print(item.get("text", json.dumps(item)))
        if result.get("isError"):
            sys.exit(1)
    else:
        print(json.dumps(result, indent=1))


def main():
    cmd = sys.argv[1]
    if cmd == "tools":
        show(rpc("tools/list", {}))
    elif cmd == "list_toolsets":
        show(rpc("tools/call", {"name": "list_toolsets", "arguments": {}}))
    elif cmd == "describe":
        show(rpc("tools/call", {"name": "describe_toolset", "arguments": {"toolset_name": sys.argv[2]}}))
    elif cmd == "call":
        args = json.loads(sys.argv[4]) if len(sys.argv) > 4 else {}
        if len(sys.argv) > 4 and sys.argv[4].startswith("@"):
            args = json.load(open(sys.argv[4][1:], encoding="utf-8"))
        show(rpc("tools/call", {"name": "call_tool", "arguments": {"toolset_name": sys.argv[2], "tool_name": sys.argv[3], "arguments": args}}))
    elif cmd == "script":
        code = open(sys.argv[2], encoding="utf-8").read()
        show(rpc("tools/call", {"name": "call_tool", "arguments": {"toolset_name": "editor_toolset.toolsets.programmatic.ProgrammaticToolset", "tool_name": "execute_tool_script", "arguments": {"script": code}}}))
    elif cmd == "raw":
        show(rpc(sys.argv[2], json.loads(sys.argv[3]) if len(sys.argv) > 3 else {}))


if __name__ == "__main__":
    main()
