#!/usr/bin/env python3
"""Host-side slicer golden generator for StarryOS slicer/mesh tests.

Generates a unit cube STL, then computes mesh-plane intersection contours
(closed polygons) at several Z heights for:
  - cube.stl        -> analytic square, exact area/perimeter check
  - suzanne.stl     -> per-layer contour area/perimeter (empirical golden)
  - benchy.stl      -> per-layer contour area/perimeter (empirical golden)

No external slicer library is used; this implements binary-STL parsing and
triangle-plane intersection directly so the golden is reproducible anywhere.
Output: JSON to stdout.
"""
import struct, sys, json, math

def read_stl(path):
    with open(path, "rb") as f:
        data = f.read()
    # ASCII STL starts with "solid" AND contains "facet"; binary otherwise.
    if data[:5] == b"solid" and b"facet" in data[:512]:
        tris = []
        verts = []
        for line in data.decode("ascii", "replace").splitlines():
            line = line.strip()
            if line.startswith("vertex"):
                _, x, y, z = line.split()
                verts.append((float(x), float(y), float(z)))
                if len(verts) == 3:
                    tris.append(tuple(verts)); verts = []
        return tris
    # binary: 80-byte header, uint32 count, then 50 bytes per triangle
    n = struct.unpack_from("<I", data, 80)[0]
    tris = []
    off = 84
    for _ in range(n):
        vals = struct.unpack_from("<12f", data, off)  # normal(3f)+3 verts(9f)
        v1 = (vals[3], vals[4], vals[5])
        v2 = (vals[6], vals[7], vals[8])
        v3 = (vals[9], vals[10], vals[11])
        tris.append((v1, v2, v3))
        off += 50
    return tris

def bounds(tris):
    xs = [v[0] for t in tris for v in t]
    ys = [v[1] for t in tris for v in t]
    zs = [v[2] for t in tris for v in t]
    return (min(xs),max(xs)),(min(ys),max(ys)),(min(zs),max(zs))

def slice_z(tris, z):
    """Return list of 2D segments (each ((x0,y0),(x1,y1))) where triangles cross plane z."""
    segs = []
    for tri in tris:
        pts = []
        for i in range(3):
            a = tri[i]; b = tri[(i+1) % 3]
            za, zb = a[2], b[2]
            if (za < z and zb >= z) or (zb < z and za >= z):
                t = (z - za) / (zb - za)
                pts.append((a[0] + t*(b[0]-a[0]), a[1] + t*(b[1]-a[1])))
        if len(pts) == 2:
            segs.append((pts[0], pts[1]))
    return segs

def contour_stats(segs):
    """Total contour perimeter (sum of segment lengths) and enclosed area
    via the shoelace on the segment set (signed sum; robust for closed loops)."""
    perim = 0.0
    area2 = 0.0
    for (x0,y0),(x1,y1) in segs:
        perim += math.hypot(x1-x0, y1-y0)
        area2 += x0*y1 - x1*y0
    return perim, abs(area2)/2.0

def write_cube_stl(path, size=1.0):
    """Axis-aligned cube [0,size]^3 as binary STL (12 triangles)."""
    s = size
    v = [(0,0,0),(s,0,0),(s,s,0),(0,s,0),(0,0,s),(s,0,s),(s,s,s),(0,s,s)]
    faces = [(0,1,2),(0,2,3),(4,6,5),(4,7,6),(0,4,5),(0,5,1),
             (1,5,6),(1,6,2),(2,6,7),(2,7,3),(3,7,4),(3,4,0)]
    with open(path, "wb") as f:
        f.write(b"\0"*80)
        f.write(struct.pack("<I", len(faces)))
        for a,b,c in faces:
            f.write(struct.pack("<3f", 0,0,0))
            for idx in (a,b,c):
                f.write(struct.pack("<3f", *v[idx]))
            f.write(struct.pack("<H", 0))

def analyse(path, name, n_layers=5):
    tris = read_stl(path)
    (bx, by, bz) = bounds(tris)
    z0, z1 = bz
    layers = []
    for i in range(1, n_layers+1):
        z = z0 + (z1 - z0) * i / (n_layers+1)
        segs = slice_z(tris, z)
        perim, area = contour_stats(segs)
        layers.append({"z": round(z,6), "n_segments": len(segs),
                       "perimeter": round(perim,6), "area": round(area,6)})
    return {"name": name, "path": path, "n_triangles": len(tris),
            "bounds": {"x": bx, "y": by, "z": bz}, "layers": layers}

def main():
    out = {}
    cube_path = sys.argv[1]
    write_cube_stl(cube_path, size=1.0)
    cube = analyse(cube_path, "cube", n_layers=3)
    # closed-form check: any horizontal slice of unit cube = 1x1 square
    cube["analytic"] = {"expected_area": 1.0, "expected_perimeter": 4.0,
                        "note": "each interior Z slice is a unit square"}
    out["cube"] = cube
    for pth, nm in [(sys.argv[2], "suzanne"), (sys.argv[3], "benchy")]:
        try:
            out[nm] = analyse(pth, nm, n_layers=5)
        except Exception as e:
            out[nm] = {"name": nm, "error": str(e)}
    print(json.dumps(out, indent=2))

if __name__ == "__main__":
    main()
