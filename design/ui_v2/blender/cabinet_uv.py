"""The cabinet's UV layout: the box unfolded into labelled panels on a 1536x1024 sheet, so ChatGPT can paint a
reference image's finish straight onto it (Joshua 2026-11-11: "send chatgpt uv maps and the reference image and have it
paste that texture onto the uv map").

Panels are stacked like an unfolded box: FRONT on top, TOP below it sharing the front edge, then BACK, BOTTOM, and the
two ENDS side by side. u runs left->right along the cabinet's length (keyhead end on the left), v runs up the sheet.
python cabinet_uv.py <out.png>   draws the layout template
"""
W, H = 1536, 1024
L, D, A = 0.917, 0.193, 0.075            # cabinet length, depth, apron height (m), as in steel_3d.py
S = 1.5                                  # sheet units per metre (sheet is 1.5 x 1.0 units)
GAP, X0 = 0.012, 0.035


def _islands():
    out, top = {}, 1.0 - 0.006
    for name, h in (("FRONT", A), ("TOP", D), ("BACK", A), ("BOTTOM", D)):
        out[name] = (X0, top - h * S, X0 + L * S, top); top -= h * S + GAP
    out["END_LEFT"] = (X0, top - A * S, X0 + D * S, top)
    out["END_RIGHT"] = (X0 + D * S + 0.05, top - A * S, X0 + 2 * D * S + 0.05, top)
    # sheet units -> 0..1 UV (u over 1.5 units wide)
    return {k: (u0 / 1.5, v0, u1 / 1.5, v1) for k, (u0, v0, u1, v1) in out.items()}


ISLANDS = _islands()


def px(box):
    """an island's pixel rectangle on the sheet (x0, y0, x1, y1), y down"""
    u0, v0, u1, v1 = box
    return round(u0 * W), round((1 - v1) * H), round(u1 * W), round((1 - v0) * H)


def uv_for(face, p):
    """UV of a point p=(x, y, z) on one of the cabinet's faces (cabinet centred at x=0, y=0, top at z=0)"""
    x, y, z = p
    fx = (x + L / 2) / L                          # 0 at the keyhead end, 1 at the changer end
    fz = (z + A) / A                              # 0 at the bottom edge of the apron, 1 at the top
    fy = (y + D / 2) / D                          # 0 at the front (player side), 1 at the back
    u0, v0, u1, v1 = ISLANDS[face]
    if face == "FRONT": a, b = fx, fz
    elif face == "TOP": a, b = fx, 1 - fy        # front edge at the top of the panel, touching FRONT
    elif face == "BACK": a, b = fx, fz
    elif face == "BOTTOM": a, b = fx, fy
    elif face == "END_LEFT": a, b = fy, fz
    else: a, b = 1 - fy, fz
    return u0 + a * (u1 - u0), v0 + b * (v1 - v0)


if __name__ == "__main__":
    import sys
    from PIL import Image, ImageDraw, ImageFont
    im = Image.new("RGB", (W, H), (0, 0, 0)); d = ImageDraw.Draw(im)
    try: f = ImageFont.truetype("arial.ttf", 22)
    except Exception: f = ImageFont.load_default()
    for k, b in ISLANDS.items():
        x0, y0, x1, y1 = px(b)
        d.rectangle((x0, y0, x1, y1), fill=(128, 128, 128), outline=(255, 255, 255), width=2)
        d.text((x0 + 10, y0 + 6), k.replace("_", " ") + ("  (keyhead end <-  -> changer end)" if x1 - x0 > 800 else ""), fill=(255, 255, 255), font=f)
    im.save(sys.argv[1]); print({k: px(b) for k, b in ISLANDS.items()})
