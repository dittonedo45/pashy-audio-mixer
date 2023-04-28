#include <iostream>
#include <vector>
#include <memory>
#include <cstdio>
extern "C" {
#include <libavformat/avformat.h>
#include <python3.8/Python.h>
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libswresample/swresample.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/avutil.h>
#include <libavutil/opt.h>
};

using namespace std;

struct b_string : public string 
{
	b_string (char* s) : string(s){}

	operator const char*()
	{
		return data();
	}
	operator char*()
	{
		return (char*)data();
	}
};

struct a_ins_mem : public exception {
	string wh;
	a_ins_mem (string g) : wh (g) {}
	const char* what ()
	{
		return wh.data();
	}
};

struct a_exception : public exception {
	virtual const char*what ()
	{
		return "Audio::Error: <main>";
	}
};

struct a_find_stream_error : public a_exception {
	virtual const char*what ()
	{
		return "Audio::Error: <a_find_stream_error>";
	}
};

struct filter_gh
{
	AVFilterGraph *fg;
	const AVFilter* f{avfilter_get_by_name ("abuffer")};
	const AVFilter* fs{avfilter_get_by_name ("abuffersink")};
	AVFilterContext *ctx1, *ctx2, *sink;
	AVCodecContext* enc;
	const AVCodec* e{avcodec_find_encoder (AV_CODEC_ID_MP3)};
	int rate{44100};
	int r;

	AVFilterContext*& get_sink ()
	{
		return sink;
	}
	AVFilterContext*& get_src (int i)
	{
		if (i)
		{
			return ctx2;
		}else{
			return ctx1;
		}
	}

	filter_gh () : fg(avfilter_graph_alloc ())
	{
		char buf[1054];
		uint64_t c_l;

		for (const int *p=e->supported_samplerates;
				p && *p<=0; p++)
		{
			if(rate<*p)
				rate=*p;
		}
		c_l=*e->channel_layouts;
		enc=avcodec_alloc_context3 (e);
		enc->time_base={1, rate};
		enc->sample_fmt=*(e->sample_fmts);
		enc->sample_rate=rate;
		enc->channel_layout=c_l;

		r=avcodec_open2(enc, e, NULL);
		if (r<0) abort ();

		snprintf (buf, 1054,
			  "time_base=%d/%d:sample_rate=%d:sample_fmt=%s:channel_layout=0x%llx",
			  1, rate, rate,
			  av_get_sample_fmt_name (*(e->sample_fmts)),
			  c_l);
		static uint8_t sfff=*(e->sample_fmts);
		r = avfilter_graph_create_filter (&ctx1,
				f, "in1", buf, 0, fg);
		if (r<0) abort ();
		r = avfilter_graph_create_filter (&ctx2,
				f, "in2", buf, 0, fg);
		if (r<0) abort ();
		r = avfilter_graph_create_filter (&sink, fs,
						  "out", NULL, NULL, fg);
		if (r<0) abort ();

		AVFilterInOut *outs = avfilter_inout_alloc ();
		AVFilterInOut *out = avfilter_inout_alloc ();
		AVFilterInOut *ins = avfilter_inout_alloc ();

		    av_opt_set_bin (sink, "sample_rates",
				    (uint8_t *) & rate,
				    sizeof (rate),
				    AV_OPT_SEARCH_CHILDREN);
		    av_opt_set_bin (sink, "channel_layouts",
				    (uint8_t *) & c_l,
				    sizeof (c_l),
				    AV_OPT_SEARCH_CHILDREN);
		    av_opt_set_bin (sink, "sample_fmts",
				    (uint8_t *) &sfff,
				    sizeof (c_l),
				    AV_OPT_SEARCH_CHILDREN);

		    outs->name = av_strdup ("in2");
		    outs->filter_ctx = ctx1;
		    outs->pad_idx = 0;
		    outs->next = out;

		    out->name = av_strdup ("in1");
		    out->filter_ctx = ctx2;
		    out->pad_idx = 0;
		    out->next = NULL;

		    ins->name = av_strdup ("out");
		    ins->filter_ctx = sink;
		    ins->pad_idx = 0;
		    ins->next = 0;

		    {
		      r =
			avfilter_graph_parse_ptr (fg, "[in1] lowpass, [in2]amerge, asetrate=44100*1.2[out]", &ins,
						  &outs, 0);
		      avfilter_inout_free (&ins);
		      avfilter_inout_free (&outs);
		    }
		    if (r<0) abort();
		    r = avfilter_graph_config (fg, NULL);
		    if (r<0) abort();
	}
	~filter_gh ()
	{
		avfilter_graph_free (&fg);
	}
} ffg;

struct Codec {
	int ret, stream_index;
	AVCodecContext* dec_ctx{NULL};
	AVCodec *d{NULL};

	operator AVCodec**(){
		return &d;
	}

	Codec (){}

	Codec(const Codec& c)
	{
		ret=c.ret;
		stream_index=c.stream_index;
		d=c.d;
		dec_ctx=c.dec_ctx;
	}

	void alloc()
	{
		dec_ctx = avcodec_alloc_context3 (d);
		if (dec_ctx==NULL)
		{
			throw a_ins_mem ("Failed to allocate Codec:");
		}
	}

	~Codec ()
	{
		if (dec_ctx)
			avcodec_free_context (&dec_ctx);
	}
};

struct OSPacket : public exception {};

class  Format {
	public:
	AVFormatContext* fmtctx{NULL};
	int ret, stream_index, rchd_end{0};
	int rate{44100};
	int channels{0};
	Codec ctx;
	SwrContext* swr{NULL};
	AVPacket* pkt{av_packet_alloc()};
	AVCodecContext* enc;

	operator int()
	{
		return stream_index;
	}
	operator AVFormatContext*()
	{
		return fmtctx;
	}
	Format ()
	{}
	void alloc_enc()
	{
		AVCodec* e=avcodec_find_encoder (
			AV_CODEC_ID_MP3);
		enc=avcodec_alloc_context3 (e);
		uint64_t c_l;

		for (const int *p=e->supported_samplerates;
				p && *p<=0; p++)
		{
			if(rate<*p)
				rate=*p;
		}
		c_l=*e->channel_layouts;
		channels=av_get_channel_layout_nb_channels (c_l);
		enc->sample_rate=rate;
		enc->channel_layout=c_l;
		enc->channels=channels;
		enc->sample_fmt=*(e->sample_fmts);
		enc->time_base={1,rate};

		int ret=avcodec_open2 (enc, e, NULL);
		if (ret<0)
			abort ();
	}

	Format (const Format& f)
	{
		fmtctx=f.fmtctx;
		ret=f.ret;
		stream_index=f.stream_index;
		ctx=f.ctx;
	}

	Format (char *str)
	{
		ret=avformat_open_input (&fmtctx,
				str, NULL, NULL);
		if (ret<0) {
			throw a_exception ();
		}
		stream_index=ret=av_find_best_stream (fmtctx,
				AVMEDIA_TYPE_AUDIO,
				-1,
				-1,
				ctx,
				NULL);
		try{
			ctx.alloc ();
			ret=avcodec_parameters_to_context(
			ctx.dec_ctx, fmtctx->streams[stream_index]->codecpar);
			if (ret<0) throw ret;
			ret=avcodec_open2 (ctx.dec_ctx, ctx.d, NULL);
			if (ret<0) throw ret;
			alloc_enc ();
			swr=swr_alloc_set_opts(NULL,
				 enc->channel_layout,
				 enc->sample_fmt,
				 enc->sample_rate,
				 ctx.dec_ctx->channel_layout,
				 ctx.dec_ctx->sample_fmt,
				 ctx.dec_ctx->sample_rate,
				 0,
				 NULL
				 );
			int ret=swr_init(swr);
			if (ret<0)
				abort ();
		} catch (a_ins_mem& exp)
		{
			cerr<<exp.what()<<endl;
			throw;
		}
	}

	void find_stream ()
	{
		ret=avformat_find_stream_info (fmtctx, NULL);
		if (ret<0) {
			throw a_find_stream_error ();
		}
	}
	private:
	void _get_packet ()
	{
		int ret(av_read_frame (fmtctx, pkt));
		if (ret==AVERROR_EOF)
			throw -1;
		if (ret<0)
			throw OSPacket();
	}
	public:
	AVPacket*& get_packet ()
	{
		while (1)
		try {
			_get_packet ();
			break;
		}catch (OSPacket& e)
		{
			throw -1;
		}catch (...)
		{
			throw;
		}
		return pkt;
	}

	int set_frame(AVPacket* pkt)
	{
		ret=avcodec_send_packet (ctx.dec_ctx, pkt);
		if (ret<0)
			throw 1;
		rchd_end=0;
		return 2;
	}

	void seek(int s)
	{
		int64_t seek_target=s*AV_TIME_BASE/1000000;
		int ret=avformat_seek_file (fmtctx, stream_index, INT64_MIN, seek_target, INT64_MAX, AVSEEK_FLAG_FRAME
				);
		if (ret<0)
			throw ret;
	}

	long duration()
	{
		return fmtctx->streams[stream_index]->duration;
	}

	AVFrame* get_frames()
	{
		if (rchd_end)
			return NULL;
		AVFrame *frame=av_frame_alloc ();
		AVFrame *fframe=av_frame_alloc ();
		ret=avcodec_receive_frame (ctx.dec_ctx, frame);
		if (ret<0)
		{
			rchd_end=1;
			av_frame_free (&frame);
			av_frame_free (&fframe);
			return NULL;
		}
		fframe->sample_rate=enc->sample_rate;
		fframe->channel_layout=enc->channel_layout;
		fframe->channels=enc->channels;
		fframe->format=enc->sample_fmt;
		av_frame_get_buffer (fframe, 0);

		swr_convert_frame (swr, fframe, frame);
		av_frame_free (&frame);
		return fframe;
	}

	~Format ()
	{
		avcodec_free_context (&enc);
		avformat_close_input (&fmtctx);
		avformat_free_context(fmtctx);
	}

};

namespace packet
{
	struct fobject {
		PyObject_HEAD
		AVPacket* pkt;
	};
	using T=PyObject*;
	T fobject_new (PyTypeObject* t, T a, T k)
	{
		return t->tp_alloc(t, 0);
	}
	int fobject_init (T t, T a, T k)
	{
		PyGILState_STATE state=PyGILState_Ensure ();
		fobject* fb=(fobject*)t;
		struct gil
		{
			PyGILState_STATE state;
			gil(){
				state=PyGILState_Ensure ();
			}
			~gil(){
				PyGILState_Release (state);
			}
		} g;
		fb->pkt=av_packet_alloc ();
		if (!fb->pkt )
		{
			PyErr_NoMemory ();
			return 1;
		}
		return 0;
	}
	void fobject_dealloc (T o)
	{
		fobject* f=(fobject*) o;
		av_packet_free (&f->pkt);
	}
	static PyTypeObject fobject_type ={
		PyVarObject_HEAD_INIT (NULL, 0)
		.tp_name="AVPacket",
		.tp_init=fobject_init,
		.tp_dealloc=fobject_dealloc,
		.tp_new=fobject_new,
		.tp_basicsize=sizeof(fobject),
		.tp_flags=Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE
	};
}

namespace f
{
	struct fobject {
		PyObject_HEAD
		Format* fmtctx;
	};

	using T=PyObject*;
	T fobject_new (PyTypeObject* t, T a, T k)
	{
		return t->tp_alloc(t, 0);
	}
	int fobject_init (T t, T a, T k)
	{
		fobject* fb=(fobject*)t;
		char *path;

		if (!PyArg_ParseTuple (a, "s", &path))
			return 1;
		PyGILState_STATE state=PyGILState_Ensure ();
		try{
			struct gil
			{
				PyGILState_STATE state;
				gil(){
					state=PyGILState_Ensure ();
				}
				~gil(){
					PyGILState_Release (state);
				}
			} g;
			fb->fmtctx=new Format(path);
		}catch(a_exception& exp)
		{
			PyErr_Format (PyExc_RuntimeError,
				exp.what ());
			return 1;
		}catch(...){
			PyErr_Format (PyExc_RuntimeError,
				"Failed to allocate.");
			return 1;
		}
		return 0;
	}
	void fobject_dealloc (T o)
	{
		fobject* f=(fobject*) o;
		delete f->fmtctx;
	}
	T get_packet (T s, T a)
	{
		AVPacket *pkt(av_packet_alloc ());
		try {
			fobject* f=(fobject*) s;
			AVPacket*& pkt=f->fmtctx->get_packet ();
			AVPacket* res=av_packet_clone(pkt);
			return PyCapsule_New(res, "_packet",
					+[](T obj)
					{
					AVPacket* p=
					(AVPacket*)
					PyCapsule_GetPointer (obj, "_packet");
					av_packet_free(&p);
					});
		}catch(...)
		{
			PyErr_Format (PyExc_EOFError,
					"reached end of file");
			return NULL;
		}
	}
	T get_frames (T s, T a)
	{
		AVPacket *pkt(av_packet_alloc ());
		T arg;
		if (!PyArg_ParseTuple (a, "O", &arg))
			return NULL;
		try {
			fobject* f=(fobject*) s;

			if (arg!=Py_None)
			{
				f->fmtctx->set_frame (
					(AVPacket*)
					PyCapsule_GetPointer(arg, "_packet")
						);
			}
			AVFrame* frame=f->fmtctx->get_frames ();
			if (!frame)
				throw 0x0;
			return PyCapsule_New(frame, "_frame",
					+[](T obj)
				{
					AVFrame* p=
					(AVFrame*)
					PyCapsule_GetPointer (obj, "_frame");
					av_frame_free(&p);
				});
		}catch(...)
		{
			PyErr_Format (PyExc_EOFError,
					"reached end of file");
			return NULL;
		}
	}
	T seek_duration (T s, T a)
	{
		int arg{0};
		if (!PyArg_ParseTuple (a, "|d", &arg))
			return NULL;
		try {
			fobject* f=(fobject*) s;
			f->fmtctx->seek (arg);
		}catch(...)
		{
			PyErr_Format (PyExc_Exception,
					"Can not seek any file so easily.");
			return NULL;
		}
		Py_RETURN_NONE;
	}
	T get_duration (T s, T a)
	{
		try {
			fobject* f=(fobject*) s;
			return PyLong_FromLong(f->fmtctx->duration ());
		}catch(...)
		{
			PyErr_Format (PyExc_Exception,
					"Can not seek any file so easily.");
			return NULL;
		}
		Py_RETURN_NONE;
	}
	static PyMethodDef methods[]={
		{"get_packet", get_packet, METH_VARARGS,
			"Get a packet"},
		{"send_frame",get_frames, METH_VARARGS,
			"Get_frame"},
		{"seek_duration",
			seek_duration, METH_VARARGS,
			"Seek AVFORMATCONTEXT"},
		{"duration",
			get_duration, METH_VARARGS,
			"get_duration AVFORMATCONTEXT"},
		{NULL, NULL, 0, NULL}
	};
	T get_frame (T s, T a)
	{
		PyObject* arg;
		int index;
		int r;
		if (!PyArg_ParseTuple (a, "Oi", &arg, &index))
			return NULL;
		AVFrame* frame=(AVFrame*)PyCapsule_GetPointer (arg,
				"_frame");
		r=av_buffersrc_add_frame (ffg.get_src (index), frame);
		if (r>=0)
		{
			AVFrame* frame=av_frame_alloc ();

			do {
				r = av_buffersink_get_frame_flags (ffg.get_sink (),
					      frame, 4);
				if (r>=0)
				{
					r=avcodec_send_frame (ffg.enc, frame);
					if (r>=0)
					{
						AVPacket* pkt=av_packet_alloc ();
						do{
							if (avcodec_receive_packet (ffg.enc,
									pkt)) break;
							fwrite(pkt->data,
								pkt->size,
								1, stdout);
							fflush(stdout);
						} while (1);
						av_packet_free (&pkt);
					}
				}
			}while (r>=0);
			av_frame_free(&frame);
		}
		Py_RETURN_NONE;
	}
	static PyTypeObject fobject_type ={
		PyVarObject_HEAD_INIT (NULL, 0)
		.tp_name="AVFormatContext",
		.tp_init=fobject_init,
		.tp_dealloc=fobject_dealloc,
		.tp_new=fobject_new,
		.tp_methods=methods,
		.tp_basicsize=sizeof(fobject),
		.tp_flags=Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE
	};

	PyMODINIT_FUNC
	PyInit_av ()
	{
		static PyMethodDef methods[]={
			{"get_frame",
			get_frame, METH_VARARGS,
			"Get the AVFrame*;"},
			{NULL, NULL, 0, NULL}
		};
		static struct PyModuleDef avv={
			PyModuleDef_HEAD_INIT,
			"audio_video",
			0, -1, methods
		};

		PyObject *ov=PyModule_Create (&avv);

		PyType_Ready (&fobject_type);
		PyType_Ready (&packet::fobject_type);

		PyModule_AddObject (ov, "Format",
				(T)&fobject_type);
		PyModule_AddObject (ov, "Packet",
				(T)&packet::fobject_type);

		return ov;
	}
}



auto main(int argsc, char **args) -> int
{
	using namespace std;
	av_log_set_callback (0x0);
	PyImport_AppendInittab ("fobject", &f::PyInit_av);
	Py_InitializeEx (0);
	Py_BytesMain (argsc, args);
	Py_Finalize ();
	return int(0);
}
