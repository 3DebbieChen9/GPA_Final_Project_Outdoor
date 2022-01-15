# GPA_Final_Project_Outdoor
110 GPA Final Project Outdoor Scene

## Check List (Basic)  15/30
- [x] Render scene correctly (5)
- [ ] Phong Shading (5)
  > Don't know what cause the problem!!!
- [ ] Direction Light Shadow (5)
  > Give up.....
- [x] Deferred Shading (5)
- [x] Normal Mapping (5)
- [ ] Bloom Effect (5)
  > It has something to do with "Phong Shading"......

## Framework
- Load Models (airplane, house, sphere)
- Build up two framebuffer
  1. `m_genTexture`
    - Store all the required data in texture (`TEXTURE_DIFFUSE`, `TEXTURE_AMBIENT`, `TEXTURE_SPECULAR`, `TEXTURE_WS_POSITION`, `TEXTURE_WS_NORMAL`, `TEXTURE_WS_TANGENT`)
  2. `m_deferred`
    - Compute the Phong Shading based on the data from the textures of `m_genTexture` (`TEXTURE_PHONG`)
    - Compute bloom lighting mask (`TEXTURE_BLOOMHDR`)
  3. m_windowFrame
    - Blur the `bloomHDR` to apply bloom effect. (`TEXTURE_FINAL`)
    - Use keyboard to switch which textures the window should display. 

## Control Manuel
### Trackball
- `w`: forward
- `s`: backward
- `a`: left
- `d`: right
- `q`: up
- `e`: down

### Move Camera / Look-at Center
- `x`: Begin to input the camera position (`m_eye`), separate each float with space.
- `c`: Begin to input the look-at center (`m_lookAtCenter`), separate each float with space.

### Texture Selection
- `1`: Final (Phong Shading + Bloom Effect)
- `2`: Diffuse
- `3`: Ambient
- `4`: Specular
- `5`: World Space Position
- `6`: World Space Normal
- `7`: World Space Tangent
- `8`: Phong Shading w/o Bloom Effect
- `9`: BloomHDR

### Normal Mapping 
- `z`: Switch whether the houses have applied Normal Mapping

### Current Texture Display
- `TEXTURE_FINAL`
  ![](https://i.imgur.com/HfGBRZZ.jpg)
- `TEXTURE_DIFFUSE`
  ![](https://i.imgur.com/04mvZjr.jpg)
- `TEXTURE_AMBIENT`
  ![](https://i.imgur.com/5KOJCaz.png)
- `TEXTURE_SPECULAR`
  ![](https://i.imgur.com/edYfkLi.png)
- `TEXTURE_WS_POSITION`
  > 我已經有用 `clamp` 處理了，但還是全白OAO

  ![](https://i.imgur.com/zdonjrD.png)
- `TEXTURE_WS_NORMAL`
  - With normal map
  ![](https://i.imgur.com/gLLt4Ay.jpg)
  - Without normal map
  ![](https://i.imgur.com/pYo7nIx.png)
- `TEXTURE_WS_TANGENT`
  ![](https://i.imgur.com/UqnGd26.png)
- `TEXTURE_PHONG`
  ![](https://i.imgur.com/AWXr3Gb.jpg)
  ![](https://i.imgur.com/Mnj53N7.jpg)
- `TEXTURE_BLOOMHDR`
  ![](https://i.imgur.com/TzK8D6v.png)