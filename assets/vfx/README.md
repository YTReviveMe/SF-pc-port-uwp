# Atmospheric VFX atlas

`source/` contains the five generated high-resolution source cards used by
the optional depth-aware volumetric renderer:

1. fire;
2. explosion;
3. smoke;
4. fog/vapor;
5. lamp halo.

Two additional temporal sheets preserve the original retail animation
layout while supplying high-resolution density to the volume renderer:

- `source/retail_fire_hq_raw.png` contains EXPL000..011 as a 4x3 sheet;
- `source/retail_fire_tall_hq_raw.png` contains FIRE0000..0015 as a 4x4
  sheet.

They were produced with the built-in OpenAI ImageGen tool from locally
extracted retail references. The originals define frame order, silhouette,
relative scale, palette and centered pivot; the source cells are never
individually cropped, recentered or normalized. The retail images themselves
are not checked in.

The images were generated specifically for this project with OpenAI ImageGen.
They contain isolated effects on black and no third-party game artwork.
`atmosphere_atlas.png` is data, not an ordinary colour image: effect cells
store depth slices in RGBA while temporal cells store four animation frames
in RGBA. `atmosphere_atlas_preview.png` shows the maximum density of each
packed cell for visual review.

Regenerate the checked-in atlas and embedded runtime header with:

```powershell
python tools/generate_volumetric_atlas.py `
  assets/vfx/source `
  assets/vfx/atmosphere_atlas.png `
  assets/vfx/atmosphere_atlas_preview.png `
  src/platform/volumetric_atlas_texture.hpp
```

Cell layout is a 4x3 grid. Fire, explosion, smoke and fog occupy row 0.
Halo starts row 1; the remaining three cells pack EXPL000..011, four
grayscale frames per RGBA cell. Row 2 packs FIRE0000..0015 in four cells.
At runtime the exact retail family and frame select the matching density,
while the original packet still supplies position, size, colour, blend mode
and visible sprite core. Ray integration adds depth without replacing or
recentering the authored animation.
