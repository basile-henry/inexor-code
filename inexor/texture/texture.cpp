/// @file Main texture loading and organizing routines.

#include "engine.h"
#include "texture/texture.h"
#include "SDL_image.h"
#include "filesystem.h"
#include "texture/modifiers.h"
#include "texture/SDL_loading.h"
#include "texture/format.h"
#include "texture/cubemap.h"
#include "texture/textureslot.h"
#include "texture/texsettings.h"
#include "texture/compressedtex.h"


void setuptexcompress()
{
    if(!hasTC || !usetexcompress) return;

    GLenum hint = GL_DONT_CARE;
    switch(texcompressquality)
    {
        case 1: hint = GL_NICEST; break;
        case 0: hint = GL_FASTEST; break;
    }
    glHint(GL_TEXTURE_COMPRESSION_HINT_ARB, hint);
}


// textureroutine.cpp
void uploadtexture(GLenum target, GLenum internal, int tw, int th, GLenum format, GLenum type, void *pixels, int pw, int ph, int pitch, bool mipmap)
{
    int bpp = formatsize(format), row = 0, rowalign = 0;
    if(!pitch) pitch = pw*bpp; 
    uchar *buf = NULL;
    if(pw!=tw || ph!=th) 
    {
        buf = new uchar[tw*th*bpp];
        scaletexture((uchar *)pixels, pw, ph, bpp, pitch, buf, tw, th);
    }
    else if(tw*bpp != pitch)
    {
        row = pitch/bpp;
        rowalign = texalign(pixels, pitch, 1);
        while(rowalign > 0 && ((row*bpp + rowalign - 1)/rowalign)*rowalign != pitch) rowalign >>= 1;
        if(!rowalign)
        {
            row = 0;
            buf = new uchar[tw*th*bpp];
            loopi(th) memcpy(&buf[i*tw*bpp], &((uchar *)pixels)[i*pitch], tw*bpp);
        }
    }
    for(int level = 0, align = 0;; level++)
    {
        uchar *src = buf ? buf : (uchar *)pixels;
        if(buf) pitch = tw*bpp;
        int srcalign = row > 0 ? rowalign : texalign(src, pitch, 1);
        if(align != srcalign) glPixelStorei(GL_UNPACK_ALIGNMENT, align = srcalign);
        if(row > 0) glPixelStorei(GL_UNPACK_ROW_LENGTH, row);
        if(target==GL_TEXTURE_1D) glTexImage1D(target, level, internal, tw, 0, format, type, src);
        else glTexImage2D(target, level, internal, tw, th, 0, format, type, src);
        if(row > 0) glPixelStorei(GL_UNPACK_ROW_LENGTH, row = 0);
        if(!mipmap || (hasGM && hwmipmap) || max(tw, th) <= 1) break;
        int srcw = tw, srch = th;
        if(tw > 1) tw /= 2;
        if(th > 1) th /= 2;
        if(!buf) buf = new uchar[tw*th*bpp];
        scaletexture(src, srcw, srch, bpp, pitch, buf, tw, th);
    }
    if(buf) delete[] buf;
}

void uploadcompressedtexture(GLenum target, GLenum subtarget, GLenum format, int w, int h, uchar *data, int align, int blocksize, int levels, bool mipmap)
{
    int hwlimit = target==GL_TEXTURE_CUBE_MAP_ARB ? hwcubetexsize : hwtexsize,
        sizelimit = levels > 1 && maxtexsize ? min(maxtexsize, hwlimit) : hwlimit;
    int level = 0;
    loopi(levels)
    {
        int size = ((w + align-1)/align) * ((h + align-1)/align) * blocksize;
        if(w <= sizelimit && h <= sizelimit)
        {
            if(target==GL_TEXTURE_1D) glCompressedTexImage1D_(subtarget, level, format, w, 0, size, data);
            else glCompressedTexImage2D_(subtarget, level, format, w, h, 0, size, data);
            level++;
            if(!mipmap) break;
        }
        if(max(w, h) <= 1) break;
        if(w > 1) w /= 2;
        if(h > 1) h /= 2;
        data += size;
    }
}
    
void setuptexparameters(int tnum, void *pixels, int clamp, int filter, GLenum format, GLenum target)
{
    glBindTexture(target, tnum);
    glTexParameteri(target, GL_TEXTURE_WRAP_S, clamp&1 ? GL_CLAMP_TO_EDGE : GL_REPEAT);
    if(target!=GL_TEXTURE_1D) glTexParameteri(target, GL_TEXTURE_WRAP_T, clamp&2 ? GL_CLAMP_TO_EDGE : GL_REPEAT);
    if(target==GL_TEXTURE_2D && hasAF && min(aniso, hwmaxaniso) > 0 && filter > 1) glTexParameteri(target, GL_TEXTURE_MAX_ANISOTROPY_EXT, min(aniso, hwmaxaniso));
    glTexParameteri(target, GL_TEXTURE_MAG_FILTER, filter && bilinear ? GL_LINEAR : GL_NEAREST);
    glTexParameteri(target, GL_TEXTURE_MIN_FILTER,
        filter > 1 ?
            (trilinear ?
                (bilinear ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_LINEAR) :
                (bilinear ? GL_LINEAR_MIPMAP_NEAREST : GL_NEAREST_MIPMAP_NEAREST)) :
            (filter && bilinear ? GL_LINEAR : GL_NEAREST));
    if(hasGM && filter > 1 && pixels && hwmipmap && !uncompressedformat(format))
        glTexParameteri(target, GL_GENERATE_MIPMAP_SGIS, GL_TRUE);
}

void createtexture(int tnum, int w, int h, void *pixels, int clamp, int filter, GLenum component, GLenum subtarget, int pw, int ph, int pitch, bool resize, GLenum format)
{
    GLenum target = textarget(subtarget), type = GL_UNSIGNED_BYTE;
    switch(component)
    {
        case GL_FLOAT_RG16_NV:
        case GL_FLOAT_R32_NV:
        case GL_RGB16F_ARB:
        case GL_RGB32F_ARB:
            if(!format) format = GL_RGB;
            type = GL_FLOAT;
            break;

        case GL_RGBA16F_ARB:
        case GL_RGBA32F_ARB:
            if(!format) format = GL_RGBA;
            type = GL_FLOAT;
            break;

        case GL_DEPTH_COMPONENT16:
        case GL_DEPTH_COMPONENT24:
        case GL_DEPTH_COMPONENT32:
            if(!format) format = GL_DEPTH_COMPONENT;
            break;

        case GL_RGB5:
        case GL_RGB8:
        case GL_RGB16:
        case GL_COMPRESSED_RGB_ARB:
        case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
            if(!format) format = GL_RGB;
            break;

        case GL_RGBA8:
        case GL_RGBA16:
        case GL_COMPRESSED_RGBA_ARB:
        case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
        case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
        case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
            if(!format) format = GL_RGBA;
            break;
    }
    if(!format) format = component;
    if(tnum) setuptexparameters(tnum, pixels, clamp, filter, format, target);
    if(!pw) pw = w;
    if(!ph) ph = h;
    int tw = w, th = h;
    bool mipmap = filter > 1 && pixels;
    if(resize && pixels) 
    {
        resizetexture(w, h, mipmap, false, target, 0, tw, th);
        if(mipmap) component = compressedformat(component, tw, th);
    }
    uploadtexture(subtarget, component, tw, th, format, type, pixels, pw, ph, pitch, mipmap); 
}

void createcompressedtexture(int tnum, int w, int h, uchar *data, int align, int blocksize, int levels, int clamp, int filter, GLenum format, GLenum subtarget)
{
    GLenum target = textarget(subtarget);
    if(tnum) setuptexparameters(tnum, data, clamp, filter, format, target);
    uploadcompressedtexture(target, subtarget, format, w, h, data, align, blocksize, levels, filter > 1); 
}

hashtable<char *, Texture> textures;

/// Registers a texture to the texture registry, so it wont be loaded twice (but looked up the other time).
Texture *registertexture(const char *name)
{
    char *key = newstring(name);
    path(key);
    Texture *t = &textures[key];
    t->name = key;
    return t;
}

/// Receives a texture from the hashtable of all loaded textures if name is equal.
/// @warning not threadsafe.
Texture *gettexture(const char *name)
{
    string tname;
    copystring(tname, name);
    return textures.access(path(tname));
}

Texture *notexture = NULL; // used as default, ensured to be loaded

int texalign(void *data, int w, int bpp)
{
    size_t address = size_t(data) | (w*bpp);
    if(address&1) return 1;
    if(address&2) return 2;
    if(address&4) return 4;
    return 8;
}

Texture *newtexture(Texture *t, const char *rname, ImageData &s, int clamp, bool mipit, bool canreduce, bool transient, int compress)
{
    if(!t) t = registertexture(rname);

    t->clamp = clamp;
    t->mipmap = mipit;
    t->type = Texture::IMAGE;
    if(transient) t->type |= Texture::TRANSIENT;
    if(!s.data)
    {
        t->type |= Texture::STUB;
        t->w = t->h = t->xs = t->ys = t->bpp = 0;
        return t;
    }
    GLenum format;
    if(s.compressed)
    {
        format = uncompressedformat(s.compressed);
        t->bpp = formatsize(format);
        t->type |= Texture::COMPRESSED;
    }
    else 
    {
        format = texformat(s.bpp);
        t->bpp = s.bpp;
    }
    if(alphaformat(format)) t->type |= Texture::ALPHA;
    t->w = t->xs = s.w;
    t->h = t->ys = s.h;

    int filter = !canreduce || reducefilter ? (mipit ? 2 : 1) : 0;
    glGenTextures(1, &t->id);
    if(s.compressed)
    {
        uchar *data = s.data;
        int levels = s.levels, level = 0;
        if(canreduce && texreduce) loopi(min(texreduce, s.levels-1))
        {
            data += s.calclevelsize(level++);
            levels--;
            if(t->w > 1) t->w /= 2;
            if(t->h > 1) t->h /= 2;
        } 
        int sizelimit = mipit && maxtexsize ? min(maxtexsize, hwtexsize) : hwtexsize;
        while(t->w > sizelimit || t->h > sizelimit)
        {
            data += s.calclevelsize(level++);
            levels--;
            if(t->w > 1) t->w /= 2;
            if(t->h > 1) t->h /= 2;
        }
        createcompressedtexture(t->id, t->w, t->h, data, s.align, s.bpp, levels, clamp, filter, s.compressed, GL_TEXTURE_2D);
    }
    else
    {
        resizetexture(t->w, t->h, mipit, canreduce, GL_TEXTURE_2D, compress, t->w, t->h);
        GLenum component = compressedformat(format, t->w, t->h, compress);
        createtexture(t->id, t->w, t->h, s.data, clamp, filter, component, GL_TEXTURE_2D, t->xs, t->ys, s.pitch, false, format);
    }
    return t;
}
   
static vec parsevec(const char *arg)
{
    vec v(0, 0, 0);
    int i = 0;
    for(; arg[0] && (!i || arg[0]=='/') && i<3; arg += strcspn(arg, "/,><"), i++)
    {
        if(i) arg++;
        v[i] = atof(arg);
    }
    if(i==1) v.y = v.z = v.x;
    return v;
}

VAR(usedds, 0, 1, 1);
VAR(scaledds, 0, 2, 4);

bool texturedata(ImageData &d, const char *tname, Slot::Tex *tex, bool msg, int *compress)
{
    const char *cmds = NULL, *file = tname;

    if(!tname)
    {
        if(!tex) return false;
        if(tex->name[0]=='<') 
        {
            cmds = tex->name;
            file = strrchr(tex->name, '>');
            if(!file) { if(msg) conoutf(CON_WARN, "could not load texture %s", tex->name); return false; }
            file++;
        }
        else file = tex->name;
        
        static string pname; //todo threadsafe?
        formatstring(pname)("%s", file);
        file = path(pname);
    }
    else if(tname[0]=='<')
    {
        cmds = tname;
        file = strrchr(tname, '>');
        if(!file) { if(msg) conoutf(CON_WARN, "could not load texture %s", tname); return false; }
        file++;
    }

    bool raw = !usedds || !compress, dds = false;
    for(const char *pcmds = cmds; pcmds;)
    {
        #define PARSETEXCOMMANDS(cmds) \
            const char *cmd = NULL, *end = NULL, *arg[4] = { NULL, NULL, NULL, NULL }; \
            cmd = &cmds[1]; \
            end = strchr(cmd, '>'); \
            if(!end) break; \
            cmds = strchr(cmd, '<'); \
            size_t len = strcspn(cmd, ":,><"); \
            loopi(4) \
            { \
                arg[i] = strchr(i ? arg[i-1] : cmd, i ? ',' : ':'); \
                if(!arg[i] || arg[i] >= end) arg[i] = ""; \
                else arg[i]++; \
            }
        PARSETEXCOMMANDS(pcmds);
        if(matchstring(cmd, len, "noff"))
        {
            if(renderpath==R_FIXEDFUNCTION) return true;
        }
        else if(matchstring(cmd, len, "ffmask") || matchstring(cmd, len, "ffskip"))
        {
            if(renderpath==R_FIXEDFUNCTION) raw = true;
        }
        else if(matchstring(cmd, len, "decal"))
        {
            if(renderpath==R_FIXEDFUNCTION && !hasTE) raw = true;
        }
        else if(matchstring(cmd, len, "dds")) dds = true;
        else if(matchstring(cmd, len, "thumbnail")) raw = true;
        else if(matchstring(cmd, len, "stub")) return canloadsurface(file);
    }

    if(msg) renderprogress(loadprogress, file);

    int flen = strlen(file);
    if(flen >= 4 && (!strcasecmp(file + flen - 4, ".dds") || (dds && !raw)))
    {
        string dfile;
        copystring(dfile, file);
        memcpy(dfile + flen - 4, ".dds", 4);
        if(!loaddds(dfile, d, raw ? 1 : (dds ? 0 : -1)) && (!dds || raw))
        {
            if(msg) conoutf(CON_WARN, "could not load texture %s", dfile);
            return false;
        }
        if(d.data && !d.compressed && !dds && compress) *compress = scaledds;
    }

    if(!d.data)
    {
        SDL_Surface *s = loadsurface(file);
        if(!s) { if(msg) conoutf(CON_WARN, "could not load texture %s", file); return false; }
        int bpp = s->format->BitsPerPixel;
        if(bpp%8 || !texformat(bpp/8)) { SDL_FreeSurface(s); conoutf(CON_WARN, "texture must be 8, 16, 24, or 32 bpp: %s", file); return false; }
        if(max(s->w, s->h) > (1<<12)) { SDL_FreeSurface(s); conoutf(CON_WARN, "texture size exceeded %dx%d pixels: %s", 1<<12, 1<<12, file); return false; }
        d.wrap(s);
    }

    if(!d.compressed) while(cmds)
    {
        PARSETEXCOMMANDS(cmds);
        if(matchstring(cmd, len, "mad")) texmad(d, parsevec(arg[0]), parsevec(arg[1]));
        else if(matchstring(cmd, len, "colorify")) texcolorify(d, parsevec(arg[0]), parsevec(arg[1]));
        else if(matchstring(cmd, len, "colormask")) texcolormask(d, parsevec(arg[0]), *arg[1] ? parsevec(arg[1]) : vec(1, 1, 1));
        else if(matchstring(cmd, len, "ffmask"))
        {
            texffmask(d, atof(arg[0]), atof(arg[1]));
            if(!d.data) break;
        }
        else if(matchstring(cmd, len, "normal")) 
        {
            int emphasis = atoi(arg[0]);
            texnormal(d, emphasis > 0 ? emphasis : 3);
        }
        else if(matchstring(cmd, len, "dup")) texdup(d, atoi(arg[0]), atoi(arg[1]));
        else if(matchstring(cmd, len, "decal")) texdecal(d);
        else if(matchstring(cmd, len, "offset")) texoffset(d, atoi(arg[0]), atoi(arg[1]));
        else if(matchstring(cmd, len, "rotate")) texrotate(d, atoi(arg[0]), tex ? tex->type : 0);
        else if(matchstring(cmd, len, "reorient")) texreorient(d, atoi(arg[0])>0, atoi(arg[1])>0, atoi(arg[2])>0, tex ? tex->type : TEX_DIFFUSE);
        else if(matchstring(cmd, len, "mix")) texmix(d, *arg[0] ? atoi(arg[0]) : -1, *arg[1] ? atoi(arg[1]) : -1, *arg[2] ? atoi(arg[2]) : -1, *arg[3] ? atoi(arg[3]) : -1);
        else if(matchstring(cmd, len, "grey")) texgrey(d);
        else if(matchstring(cmd, len, "blur"))
        {
            int emphasis = atoi(arg[0]), repeat = atoi(arg[1]);
            texblur(d, emphasis > 0 ? clamp(emphasis, 1, 2) : 1, repeat > 0 ? repeat : 1);
        }
        else if(matchstring(cmd, len, "premul")) texpremul(d);
        else if(matchstring(cmd, len, "agrad")) texagrad(d, atof(arg[0]), atof(arg[1]), atof(arg[2]), atof(arg[3]));
        else if(matchstring(cmd, len, "compress") || matchstring(cmd, len, "dds")) 
        { 
            int scale = atoi(arg[0]);
            if(scale <= 0) scale = scaledds;
            if(compress) *compress = scale;
        }
        else if(matchstring(cmd, len, "nocompress"))
        {
            if(compress) *compress = -1;
        }
        else if(matchstring(cmd, len, "thumbnail"))
        {
            int w = atoi(arg[0]), h = atoi(arg[1]);
            if(w <= 0 || w > (1<<12)) w = 64;
            if(h <= 0 || h > (1<<12)) h = w;
            if(d.w > w || d.h > h) scaleimage(d, w, h);
        }
        else if(matchstring(cmd, len, "ffskip"))
        {
            if(renderpath==R_FIXEDFUNCTION) break;
        }
    }

    return true;
}

void loadalphamask(Texture *t)
{
    if(t->alphamask || !(t->type&Texture::ALPHA)) return;
    ImageData s;
    if(!texturedata(s, t->name, NULL, false) || !s.data || s.compressed) return;
    t->alphamask = new uchar[s.h * ((s.w+7)/8)];
    uchar *srcrow = s.data, *dst = t->alphamask-1;
    loop(y, s.h)
    {
        uchar *src = srcrow+s.bpp-1;
        loop(x, s.w)
        {
            int offset = x%8;
            if(!offset) *++dst = 0;
            if(*src) *dst |= 1<<offset;
            src += s.bpp;
        }
        srcrow += s.pitch;
    }
}

/// @param clamp
/// @param mipit specifies whether mipmap (lower quality versions; usually used when far away or small) textures should be created.
/// @param msg specifies whether a renderprogress bar should be displayed while loading. Always off if threadsafe = true.
/// @param threadsafe if true, the texture wont be automatically registerd to the global texture registry,
///        you need to check whether it is loaded via gettexture beforehand in a nonthreaded environment and register it afterwards
///        with registertexture.
Texture *textureload(const char *name, int clamp, bool mipit, bool msg, bool threadsafe)
{
    Texture *t;

    if(!threadsafe)
    {
        t = gettexture(name);
        if(t) return t;
    }
    else t = new Texture;

    string tname;
    copystring(tname, name);
    int compress = 0;
    ImageData s;
    if(texturedata(s, tname, NULL, msg && !threadsafe, &compress)) return newtexture(threadsafe ? t : NULL, tname, s, clamp, mipit, false, false, compress);
    return notexture;
}

bool settexture(const char *name, int clamp)
{
    Texture *t = textureload(name, clamp, true, false);
    glBindTexture(GL_TEXTURE_2D, t->id);
    return t != notexture;
}

void cleanuptexture(Texture *t)
{
    DELETEA(t->alphamask);
    if(t->id) { glDeleteTextures(1, &t->id); t->id = 0; }
    if(t->type&Texture::TRANSIENT) textures.remove(t->name); 
}

void cleanuptextures()
{
    clearenvmaps();
    cleanupslots();
    cleanupvslots();
    cleanupmaterialslots();
    
    enumerate(textures, Texture, tex, cleanuptexture(&tex));
}

bool reloadtexture(const char *name)
{
    Texture *t = gettexture(name);
    if(t) return reloadtexture(*t);
    return true;
}

bool reloadtexture(Texture &tex)
{
    if(tex.id) return true;
    switch(tex.type&Texture::TYPE)
    {
        case Texture::IMAGE:
        {
            int compress = 0;
            ImageData s;
            if(!texturedata(s, tex.name, NULL, true, &compress) || !newtexture(&tex, NULL, s, tex.clamp, tex.mipmap, false, false, compress)) return false;
            break;
        }

        case Texture::CUBEMAP:
            if(!cubemaploadwildcard(&tex, NULL, tex.mipmap, true)) return false;
            break;
    }    
    return true;
}

void reloadtex(char *name)
{
    Texture *t = gettexture(name);
    if(!t) { conoutf(CON_ERROR, "texture %s is not loaded", name); return; }
    if(t->type&Texture::TRANSIENT) { conoutf(CON_ERROR, "can't reload transient texture %s", name); return; }
    DELETEA(t->alphamask);
    Texture oldtex = *t;
    t->id = 0;
    if(!reloadtexture(*t))
    {
        if(t->id) glDeleteTextures(1, &t->id);
        *t = oldtex;
        conoutf(CON_ERROR, "failed to reload texture %s", name);
    }
}

COMMAND(reloadtex, "s");

void reloadtextures()
{
    int reloaded = 0;
    enumerate(textures, Texture, tex, 
    {
        loadprogress = float(++reloaded)/textures.numelems;
        reloadtexture(tex);
    });
    loadprogress = 0;
}

bool loadimage(const char *filename, ImageData &image)
{
    SDL_Surface *s = loadsurface(path(filename, true));
    if(!s) return false;
    image.wrap(s);
    return true;
}

// registry. loading
/// textureslot: texdata,  newtexture, textures.access
// cubemap: texaccess + texdata + createcompressed
// additional: texaccess