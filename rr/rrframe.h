/* Copyright (C)2004 Landmark Graphics
 *
 * This library is free software and may be redistributed and/or modified under
 * the terms of the wxWindows Library License, Version 3 or (at your option)
 * any later version.  The full license is in the LICENSE.txt file included
 * with this distribution.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * wxWindows Library License for more details.
 */

#ifndef __RRFRAME_H
#define __RRFRAME_H

#include "rr.h"
#ifdef _WIN32
#define XDK
#endif
#include "fbx.h"
#include "hpjpeg.h"
#include "rrmutex.h"
#include "rrlog.h"
#include "rrutil.h"
#include <string.h>

static int jpegsub[RR_SUBSAMPOPT]={HPJ_444, HPJ_422, HPJ_411};

// Bitmap flags
#define RRBMP_BOTTOMUP   1  // Bottom-up bitmap (as opposed to top-down)
#define RRBMP_BGR        2  // BGR or BGRA pixel order
#define RRBMP_ALPHAFIRST 4  // BGR buffer is really ABGR, and RGB buffer is really ARGB

// Uncompressed bitmap

class rrframe
{
	public:

	rrframe(bool primary=true) : _bits(NULL), _pitch(0), _pixelsize(0),
		_flags(0), _primary(primary)
	{
		memset(&_h, 0, sizeof(rrframeheader));
		_ready.lock();
	}

	virtual ~rrframe(void)
	{
		if(_bits && _primary) delete [] _bits;
	}

	void init(rrframeheader *h, int pixelsize, int flags=RRBMP_BGR)
	{
		if(!h) throw(rrerror("rrframe::init", "Invalid argument"));
		_flags=flags;
		h->size=h->framew*h->frameh*pixelsize;
		checkheader(h);
		if(pixelsize<3 || pixelsize>4) _throw("Only true color bitmaps are supported");
		if(h->framew!=_h.framew || h->frameh!=_h.frameh || pixelsize!=_pixelsize
		|| !_bits)
		{
			if(_bits) delete [] _bits;
			errifnot(_bits=new unsigned char[h->framew*h->frameh*pixelsize+1]);
			_pixelsize=pixelsize;  _pitch=pixelsize*h->framew;
		}
		memcpy(&_h, h, sizeof(rrframeheader));
	}

	rrframe *gettile(int x, int y, int w, int h)
	{
		rrframe *f;
		if(!_bits || !_pitch || !_pixelsize) _throw("Frame not initialized");
		if(x<0 || y<0 || w<1 || h<1 || (x+w)>_h.width || (y+h)>_h.height)
			throw rrerror("rrframe::gettile", "Argument out of range");
		errifnot(f=new rrframe(false));
		f->_h=_h;
		f->_h.height=h;
		f->_h.width=w;
		f->_h.y=y;
		f->_h.x=x;
		f->_pixelsize=_pixelsize;
		f->_flags=_flags;
		f->_pitch=_pitch;
		bool bu=(_flags&RRBMP_BOTTOMUP);
		f->_bits=&_bits[_pitch*(bu? _h.height-y-h:y)+_pixelsize*x];
		return f;
	}

	void zero(void)
	{
		if(!_h.frameh || !_pitch) return;
		memset(_bits, 0, _pitch*_h.frameh);
	}

	bool tileequals(rrframe *last, int x, int y, int w, int h)
	{
		bool bu=(_flags&RRBMP_BOTTOMUP);
		if(x<0 || y<0 || w<1 || h<1 || (x+w)>_h.width || (y+h)>_h.height)
			throw rrerror("rrframe::tileequals", "Argument out of range");
		if(last && _h.width==last->_h.width && _h.height==last->_h.height
		&& _h.framew==last->_h.framew && _h.frameh==last->_h.frameh
		&& _h.qual==last->_h.qual && _h.subsamp==last->_h.subsamp
		&& _pixelsize==last->_pixelsize && _h.winid==last->_h.winid
		&& _h.dpynum==last->_h.dpynum && _bits
		&& _h.flags==last->_h.flags  // left & right eye can't be compared
		&& last->_bits)
		{
			unsigned char *newbits=&_bits[_pitch*(bu? _h.height-y-h:y)+_pixelsize*x];
			unsigned char *oldbits=&last->_bits[last->_pitch*(bu? _h.height-y-h:y)+_pixelsize*x];
			for(int i=0; i<h; i++)
			{
				if(memcmp(&newbits[_pitch*i], &oldbits[last->_pitch*i], _pixelsize*w))
					return false;
			}
			return true;
		}
		return false;
	}

	void ready(void) {_ready.unlock();}
	void waituntilready(void) {_ready.lock();}
	void complete(void) {_complete.unlock();}
	void waituntilcomplete(void) {_complete.lock();}

	rrframe& operator= (rrframe& f)
	{
		if(this!=&f && f._bits)
		{
			if(f._bits)
			{
				init(&f._h, f._pixelsize);
				memcpy(_bits, f._bits, f._h.framew*f._h.frameh*f._pixelsize);
			}
		}
		return *this;
	}

	rrframeheader _h;
	unsigned char *_bits;
	int _pitch, _pixelsize, _flags;

	protected:

	void dumpheader(rrframeheader *h)
	{
		rrout.print("h->size    = %lu\n", h->size);
		rrout.print("h->winid   = 0x%.8x\n", h->winid);
		rrout.print("h->dpynum  = %d\n", h->dpynum);
		rrout.print("h->framew  = %d\n", h->framew);
		rrout.print("h->frameh  = %d\n", h->frameh);
		rrout.print("h->width   = %d\n", h->width);
		rrout.print("h->height  = %d\n", h->height);
		rrout.print("h->x       = %d\n", h->x);
		rrout.print("h->y       = %d\n", h->y);
		rrout.print("h->qual    = %d\n", h->qual);
		rrout.print("h->subsamp = %d\n", h->subsamp);
		rrout.print("h->flags   = %d\n", h->flags);
	}

	void checkheader(rrframeheader *h)
	{
		if(!h || h->framew<1 || h->frameh<1 || h->width<1 || h->height<1
		|| h->x+h->width>h->framew || h->y+h->height>h->frameh)
			throw(rrerror("rrframe::checkheader", "Invalid header"));
	}

	rrmutex _ready;
	rrmutex _complete;
	friend class rrjpeg;
	bool _primary;
};

// Compressed JPEG

class rrjpeg : public rrframe
{
	public:

	rrjpeg(void) : rrframe(), _hpjhnd(NULL)
	{
		if(!(_hpjhnd=hpjInitCompress())) _throw(hpjGetErrorStr());
		_pixelsize=3;
	}

	~rrjpeg(void)
	{
		if(_hpjhnd) hpjDestroy(_hpjhnd);
	}

	rrjpeg& operator= (rrframe& b)
	{
		int hpjflags=0;
		if(!b._bits) _throw("Bitmap not initialized");
		if(b._pixelsize<3 || b._pixelsize>4) _throw("Only true color bitmaps are supported");
		if(b._h.qual>100 || b._h.subsamp>RR_SUBSAMPOPT-1)
			throw(rrerror("JPEG compressor", "Invalid argument"));
		init(&b._h);
		if(b._flags&RRBMP_BOTTOMUP) hpjflags|=HPJ_BOTTOMUP;
		if(b._flags&RRBMP_BGR) hpjflags|=HPJ_BGR;
		unsigned long size;
		hpj(hpjCompress(_hpjhnd, b._bits, b._h.width, b._pitch, b._h.height, b._pixelsize,
			_bits, &size, jpegsub[b._h.subsamp], b._h.qual, hpjflags));
		_h.size=(unsigned int)size;
		return *this;
	}

	void init(rrframeheader *h, int pixelsize)
	{
		init(h);
	}

	void init(rrframeheader *h)
	{
		checkheader(h);
		if(h->framew!=_h.framew || h->frameh!=_h.frameh || !_bits)
		{
			if(_bits) delete [] _bits;
			errifnot(_bits=new unsigned char[HPJBUFSIZE(h->framew, h->frameh)]);
		}
		memcpy(&_h, h, sizeof(rrframeheader));
	}

	private:

	hpjhandle _hpjhnd;
	friend class rrfb;
};

// Bitmap created from shared graphics memory

class rrfb : public rrframe
{
	public:

	rrfb(Display *dpy, Window win) : rrframe()
	{
		if(!dpy || !win) throw(rrerror("rrfb::rrfb", "Invalid argument"));
		XFlush(dpy);
		init(DisplayString(dpy), win);
	}

	rrfb(char *dpystring, Window win) : rrframe()
	{
		init(dpystring, win);
	}

	void init(char *dpystring, Window win)
	{
		_hpjhnd=NULL;
		memset(&_fb, 0, sizeof(fbx_struct));
		if(!dpystring || !win) throw(rrerror("rrfb::init", "Invalid argument"));
		if(!(_wh.dpy=XOpenDisplay(dpystring)))
			throw(rrerror("rrfb::init", "Could not open display"));
		_wh.win=win;
	}

	~rrfb(void)
	{
		if(_fb.bits) fbx_term(&_fb);  if(_bits) _bits=NULL;
		if(_hpjhnd) hpjDestroy(_hpjhnd);
		if(_wh.dpy) XCloseDisplay(_wh.dpy);
	}

	void init(rrframeheader *h)
	{
		checkheader(h);
		fbx(fbx_init(&_fb, _wh, h->framew, h->frameh, 1));
		if(h->framew>_fb.width || h->frameh>_fb.height)
		{
			XSync(_wh.dpy, False);
			fbx(fbx_init(&_fb, _wh, h->framew, h->frameh, 1));
		}
		memcpy(&_h, h, sizeof(rrframeheader));
		if(_h.framew>_fb.width) _h.framew=_fb.width;
		if(_h.frameh>_fb.height) _h.frameh=_fb.height;
		_pixelsize=fbx_ps[_fb.format];  _pitch=_fb.pitch;  _bits=(unsigned char *)_fb.bits;
		_flags=0;
		if(fbx_bgr[_fb.format]) _flags|=RRBMP_BGR;
		if(fbx_alphafirst[_fb.format]) _flags|=RRBMP_ALPHAFIRST;
	}

	rrfb& operator= (rrjpeg& f)
	{
		int hpjflags=0;
		if(!f._bits || f._h.size<1)
			_throw("JPEG not initialized");
		init(&f._h);
		if(!_fb.xi) _throw("Bitmap not initialized");
		if(fbx_bgr[_fb.format]) hpjflags|=HPJ_BGR;
		if(fbx_alphafirst[_fb.format]) hpjflags|=HPJ_ALPHAFIRST;
		int width=min(f._h.width, _fb.width-f._h.x);
		int height=min(f._h.height, _fb.height-f._h.y);
		if(width>0 && height>0 && f._h.width<=width && f._h.height<=height)
		{
			if(!_hpjhnd)
			{
				if((_hpjhnd=hpjInitDecompress())==NULL) throw(rrerror("rrfb::decompressor", hpjGetErrorStr()));
			}
			hpj(hpjDecompress(_hpjhnd, f._bits, f._h.size, (unsigned char *)&_fb.bits[_fb.pitch*f._h.y+f._h.x*fbx_ps[_fb.format]],
				width, _fb.pitch, height, fbx_ps[_fb.format], hpjflags));
		}
		return *this;
	}

	void redraw(void)
	{
		if(_flags&RRBMP_BOTTOMUP)
		{
			for(int i=0; i<_fb.height; i++)
				fbx(fbx_awrite(&_fb, 0, _fb.height-i-1, 0, i, 0, 1));
			fbx(fbx_sync(&_fb));
		}
		else
			fbx(fbx_write(&_fb, 0, 0, 0, 0, _fb.width, _fb.height));
	}

	void draw(void)
	{
		int w=_h.width, h=_h.height;
		XWindowAttributes xwa;
		if(!XGetWindowAttributes(_wh.dpy, _wh.win, &xwa))
		{
			rrout.print("Failed to get window attributes\n");
			return;
		}
		if(_h.x+_h.width>xwa.width || _h.y+_h.height>xwa.height)
		{
			rrout.print("WARNING: bitmap (%d,%d) at (%d,%d) extends beyond window (%d,%d)\n",
				_h.width, _h.height, _h.x, _h.y, xwa.width, xwa.height);
			w=min(_h.width, xwa.width-_h.x);
			h=min(_h.height, xwa.height-_h.y);
		}
		if(_h.x+_h.width<=_fb.width && _h.y+_h.height<=_fb.height)
		fbx(fbx_write(&_fb, _h.x, _h.y, _h.x, _h.y, w, h));
	}

	void drawtile(int x, int y, int w, int h)
	{
		if(x<0 || w<1 || (x+w)>_h.framew || y<0 || h<1 || (y+h)>_h.frameh)
			return;
		if(_flags&RRBMP_BOTTOMUP)
		{
			for(int i=0; i<h; i++)
				fbx(fbx_awrite(&_fb, x, _fb.height-i-y-1, x, i+y, w, 1));
		}
		else
		fbx(fbx_awrite(&_fb, x, y, x, y, w, h));
	}

	void sync(void) {fbx(fbx_sync(&_fb));}

	private:

	fbx_wh _wh;
	fbx_struct _fb;
	hpjhandle _hpjhnd;
};

#endif
