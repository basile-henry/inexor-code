///  Texturesets are used to load texture-stacks asynchronously
///
///  Author: Malte "a_teammate" Haase
///  Date:   15.03.2015
///

///// WIP FLAG WIP FLAG
///
/// Inexors routine load and process ingame-textures depends on 3 structures:
/// VSlot       - A Structure holding general info about scale, shadersettings, rotation, ..
/// Slot        - Basicly thats a "Texture" as you would find it ingame in the texturebrowser.
///               It consists of minimum 1 image (the diffuse-file) and various others providing special info (height,..).
///               Every Slot has a VSlot.
/// textureset  - A Set of textures/Slots to make loading easier.
///
/// The currently visible stack of ingame-textures (Slots) is vector slots.
///// Just used to sync atm, with all stuff thrown in just so its here

#include "engine.h"
#include "texture.h"

namespace inexor {
    namespace textureset {

        static VSlot *reassignvslot(Slot &owner, VSlot *vs)
        {
            owner.variants = vs;
            while (vs)
            {
                vs->slot = &owner;
                vs->linked = false;
                vs = vs->next;
            }
            return owner.variants;
        }

        static VSlot *emptyvslot(Slot &owner)
        {
            int offset = 0;
            loopvrev(slots) if (slots[i]->variants) { offset = slots[i]->variants->index + 1; break; }
            for (int i = offset; i < vslots.length(); i++) if (!vslots[i]->changed) return reassignvslot(owner, vslots[i]);
            return vslots.add(new VSlot(&owner, vslots.length()));
        }

        const char *jsontextures[8] = { "diffuse", "other", "decal", "normal", "glow", "spec", "depth", "envmap" };

        /// 
        class textureset
        {
        private:

            /// One entry to the set
            class texentry
            {
            public:
                /// A bitmask containing which of the 8 textures of the slot already have been loaded.
                int loadmask = 0;

                /// Whether this Slot is usable ingame.
                bool mounted = false;

                /// Whether or not this Slot is actually just a pointer to another occurence
                //bool reference = false;

                /// The Texture-Slot
                Slot *tex;

                texentry(Slot *s)
                {
                    tex = s;
                }
            };

        public:
            /// All included Slots.
            vector<texentry *>texs;

            /// Adds a texture to the set from a JSON (Object).
            void addtexture(JSON *j);

            /// Adds a texture to the set from a json-file.
            void addtexture(const char *filename);

            /// Checks if textures may do not need to be loaded since they are already stored somewhere
            void checkload();

            /// Loads all textures to memory.
            /// This function is threadsafe.
            /// @usage 1. checkload (not threaded) 2. load (threaded) 3. registerload (not threaded)
            void load();

            /// Saves loaded textures to the texture registry.
            void registerload();

            /// Add this textureset to the current texture stack of ingame visible textures.
            /// @param initial if true this textureset becomes the first only one.
            void mount(bool initial);

            /// Mounts remaining textures
            void mountremaining();

            /// Removes this textureset from the current stack of ingame visible textures.
            /// Attention: all following slots will be change its position and hence this has an visual impact ingame! TODO!!
            void unmount();

            void echoall()
            {
                loopv(texs) {
                    if(texs[i]->tex->sts.length())  conoutf("tex %d: %s", i, texs[i]->tex->sts.last().name);
                }
            }
        };

        void textureset::addtexture(JSON *j)
        {
            if (!j || texs.length() >= 0x10000) return;

            Slot &s = *texs.add(new texentry(new Slot(texs.length())))->tex;
            s.loaded = false;
            loopi(8) //check for all 8 kind of textures
            {
                JSON *sub = j->getitem(jsontextures[i]);
                if (!sub) continue;
                char *name = sub->valuestring;
                if (!*name) continue;
                s.texmask |= 1 << i;
                Slot::Tex &st = s.sts.add();
                st.type = i;
                st.combined = -1;
                st.t = NULL;

                if (strpbrk(name, "/\\") && *sub->currentdir) copystring(st.name, makerelpath(sub->currentdir, name)); //path relative to current folder
                else copystring(st.name, name);
                path(st.name);
            }
            if(!s.sts.length()) return; // no textures found

            setslotshader(s, j->getitem("shader")); // TODO

            //Other Parameters:
            JSON *scale = j->getitem("scale"), *rot = j->getitem("rotation"),
                *xoff = j->getitem("xoffset"), *yoff = j->getitem("yoffset");

            VSlot &vs = *emptyvslot(s);
            vs.reset();
            vs.rotation = rot ? clamp(rot->valueint, 0, 5) : 0;
            vs.xoffset = xoff ? max(xoff->valueint, 0) : 0;
            vs.yoffset = yoff ? max(yoff->valueint, 0) : 0;
            vs.scale = scale ? scale->valuefloat : 1;
            propagatevslot(&vs, (1 << VSLOT_NUM) - 1);
        }

        void textureset::addtexture(const char *filename)
        {
            if (!filename || !*filename) return;
            JSON *j = loadjson(filename);
            if (!j) {
                conoutf("could not load texture definition %s", filename); return;
            }

            addtexture(j);
            delete j;
        }

        void textureset::checkload()
        {
            loopv(texs)
            {
                texentry *t = texs[i];
                int diff = t->tex->texmask & ~t->loadmask; // All textures which havent been loaded yet
                if(diff) loopi(8) //TODO REPLACE WITH TEX_NUM
                { //TODO BREAK IF TOO MUCH OUT OF STS
                    if(!(diff & (1 << i))) continue; // not in diff
                    Slot::Tex &curimg = t->tex->sts[i];
                    curimg.t = gettexture(curimg.name);
                    if(curimg.t) t->loadmask |= 1 << i; //save to loadmask on success
                }
            }
        }

        void textureset::load()
        {
        
        }

        void textureset::mount(bool initial = false)
        {
            if(initial) texturereset(0);
            loopv(texs)
            {
                slots.add(texs[i]->tex);
                texs[i]->mounted = true;
            }
        }

        void textureset::unmount()
        {
            if(!texs[0]) return;
            int start = 0;
            loopv(slots)
            {
                if(slots[i] == texs[0]->tex) start = i;
            }
            
            texturereset(start, texs.length());

            loopv(texs) texs[i]->mounted = false;
        }

        /// Creates a textureset with all textures from a "textures" child of given JSON structure.
        /// @sideeffects allocates memory for a new textureset
        textureset *newtextureset(JSON *parent)
        {
            if (!parent) return NULL;
            JSON *j = parent->getitem("textures");
            if(!j) return NULL;
            textureset *t = new textureset();

            loopi(j->numchilds())
            {
                const char *name = j->getstring(i);
                defformatstring(fn) ("%s", name);
                if (strpbrk(name, "/\\")) formatstring(fn)("%s", makerelpath(j->currentdir, name)); //relative path to current folder
                t->addtexture(fn);
            }
            return t;
        }

        ////// Original Loading /////

        const struct slottex
        {
            const char *name;
            int id;
        } slottexs[] =
        {
            { "c", TEX_DIFFUSE },
            { "u", TEX_UNKNOWN },
            { "d", TEX_DECAL },
            { "n", TEX_NORMAL },
            { "g", TEX_GLOW },
            { "s", TEX_SPEC },
            { "z", TEX_DEPTH },
            { "e", TEX_ENVMAP }
        };

        int findslottex(const char *name)
        {
            loopi(sizeof(slottexs) / sizeof(slottex))
            {
                if (!strcmp(slottexs[i].name, name)) return slottexs[i].id;
            }
            return -1;
        }

        void texture(char *type, char *name, int *rot, int *xoffset, int *yoffset, float *scale)
        {
            if (slots.length() >= 0x10000) return;
            static int lastmatslot = -1;
            int tnum = findslottex(type), matslot = findmaterial(type);
            if (tnum < 0) tnum = atoi(type);
            if (tnum == TEX_DIFFUSE) lastmatslot = matslot;
            else if (lastmatslot >= 0) matslot = lastmatslot;
            else if (slots.empty()) return;
            Slot &s = matslot >= 0 ? lookupmaterialslot(matslot, false) : *(tnum != TEX_DIFFUSE ? slots.last() : slots.add(new Slot(slots.length())));
            s.loaded = false;
            s.texmask |= 1 << tnum;
            if (s.sts.length() >= 8) conoutf(CON_WARN, "warning: too many textures in slot %d", slots.length() - 1);
            Slot::Tex &st = s.sts.add();
            st.type = tnum;
            st.combined = -1;
            st.t = NULL;
            if (name && strpbrk(name, "/\\")) copystring(st.name, makerelpath(getcurexecdir(), name)); //relative path to current folder
            else copystring(st.name, name);
            path(st.name);
            if (tnum == TEX_DIFFUSE)
            {
                setslotshader(s);
                VSlot &vs = matslot >= 0 ? lookupmaterialslot(matslot, false) : *emptyvslot(s);
                vs.reset();
                vs.rotation = clamp(*rot, 0, 5);
                vs.xoffset = max(*xoffset, 0);
                vs.yoffset = max(*yoffset, 0);
                vs.scale = *scale <= 0 ? 1 : *scale;
                propagatevslot(&vs, (1 << VSLOT_NUM) - 1);
            }
            //conoutf(CON_WARN, "old texture loaded, should it be converted?")
        }
        COMMAND(texture, "ssiiif");



    }
}