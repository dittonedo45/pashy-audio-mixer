# You compile like so
## On bash Type or Copy this snipplet:
```zsh
g++ main.cpp -lavformat -std=c++11 `pkg-config --cflags --libs python-3.8-embed libavcodec libavutil libswresample libavfilter libavutil libavformat`
```

## Enjoy your tracks, after typing:
```sh
./a.out ./res.py /storage/sdcard1/BUSINESS\ IS\ BUSINESS\ Album/*.mp3 | play -qtmp3 -
```

# Am feeling lazy, to create a proper (auto compilation) script or setup.

#C++ file!!
```cpp
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
PyObject* Format_EOF(0);

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
	private:
	const AVFilter* f{avfilter_get_by_name ("abuffer")};
	const AVFilter* fs{avfilter_get_by_name ("abuffersink")};
	const AVCodec* e{avcodec_find_encoder (AV_CODEC_ID_MP3)};
	AVFilterInOut *ins{avfilter_inout_alloc ()};
	AVFilterInOut *outs{inout_layers(2)};

	public:
	AVFilterGraph *fg;
	AVFilterContext *ctx[2], *sink;
	AVCodecContext* enc;
	int rate{44100};
	char buf[1054];
	uint64_t c_l;
	int r;

	AVFilterContext*& get_sink ()
	{
		return sink;
	}
	AVFilterContext*& get_src (int i)
	{
		if (i)
		{
			return ctx[1];
		}else{
			return ctx[0];
		}
	}

	void stage_get_src (int index, AVFrame* frame)
	{
		int ret;
		ret=av_buffersrc_add_frame (ctx[index], frame);
		if (ret<0)
		{
			throw ret;
		}
	}

	AVFrame* frame=av_frame_alloc ();

	void stage_get_sink ()
	{
		r=av_buffersink_get_frame (sink,
				frame);
		if (r<0)
		{
			throw r;
		}
	}

	void stage_send_to_encoder ()
	{
		r=avcodec_send_frame (enc, frame);
		if (r<0)
			throw r;
	}

	AVPacket* stage_get_packet ()
	{
		AVPacket* pkt;
		pkt=av_packet_alloc ();

		r=avcodec_receive_packet (enc, pkt);
		if (r<0)
			throw r;
		return pkt;
	}

	private:
	AVFilterInOut *inout_layers(int n)
	{
		AVFilterInOut* res=avfilter_inout_alloc ();
		AVFilterInOut* sp=res;
		AVFilterInOut* p;

		for (int i(0); i<n; i++)
		{
			p=res;
			res=avfilter_inout_alloc ();
			p->next=res;
		}
		return sp;
	}

	void fstage_alloc_encoder ()
	{
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
		if (r<0)
			throw r;
	}

	void fstage_prepare_inputs ()
	{
		AVFilterInOut* pp=outs, *o;
		for (AVFilterContext** p=ctx; p<&ctx[2]; p++)
		{
			char pbuf[1054];
			snprintf (pbuf, 1054,
				"in%d", (&ctx[2])-p);
			r = avfilter_graph_create_filter (p,
					f, pbuf, buf, 0, fg);
			if (r<0)
			{
				throw r;
			}
			pp->name=av_strdup (pbuf);
			pp->filter_ctx=*p;
			pp->pad_idx=0;
			pp=pp->next;
		}
	}



	void fstage_prepare_inputs_layout ()
	{
		snprintf (buf, 1054,
			  "time_base=%d/%d:sample_rate=%d:sample_fmt=%s:channel_layout=0x%llx",
			  1, rate, rate,
			  av_get_sample_fmt_name (*(e->sample_fmts)),
			  c_l);

	}

	void fstage_prepare_output ()
	{
		static uint8_t sfff=*(e->sample_fmts);
		r = avfilter_graph_create_filter (&sink, fs,
						  "out", NULL, NULL, fg);
		if (r<0)
		{
			throw r;
		}

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

	}


	void fstage_prepare_ins_and_outs (char *str)
	{
	    ins->name = av_strdup ("out");
	    ins->filter_ctx = sink;
	    ins->pad_idx = 0;
	    ins->next = 0;

	    {
	      r =
		avfilter_graph_parse_ptr (fg, str, &ins,
					  &outs, 0);
	      avfilter_inout_free (&ins);
	      avfilter_inout_free (&outs);
	    }
	    if (r<0)
	    {
		    throw r;
	    }
	}

	void fstage_filter_config ()
	{
	    r = avfilter_graph_config (fg, NULL);
	    if (r<0)
		    throw r;
	}

	void fstage_filter_alloc ()
	{
		fg=avfilter_graph_alloc ();
	}

	public:
	filter_gh (int numi, int numo,
			b_string str) 
	{
		try {
			fstage_filter_alloc ();
			fstage_alloc_encoder ();

			fstage_prepare_inputs_layout ();
			fstage_prepare_inputs ();

			fstage_prepare_output ();

			fstage_prepare_ins_and_outs (str);
			fstage_filter_config ();
		} catch (int& r)
		{
			abort ();
			throw r;
		}
	}
	~filter_gh ()
	{
		avfilter_graph_free (&fg);
	}
};

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
		if (!d) abort ();
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

class Format
{
	public:

	AVFormatContext* fmtctx{NULL};
	string filepath;

	int ret, stream_index;
	Codec ctx;
	SwrContext* swr{NULL};

	int rate{44100};
	int channels{2};
	AVPacket* pkt{av_packet_alloc()};
	AVFrame *frame{av_frame_alloc ()};
	AVFrame *fframe;

	AVCodecContext* enc;
	const AVCodec* e{avcodec_find_encoder (AV_CODEC_ID_MP3)};


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

	void stage_alloc_encoder ()
	{
		enc=avcodec_alloc_context3 (e);
		if (!enc)
		{
			throw 1;
		}
	}
	void stage_encoder_set_parameters ()
	{
		uint64_t channel_layout;

		for (const int *p=e->supported_samplerates;
				p && *p<=0; p++)
		{
			if(rate<*p)
				rate=*p;
		}
		channel_layout=*e->channel_layouts;
		channels=av_get_channel_layout_nb_channels (channel_layout);
		enc->sample_rate=rate;
		enc->channel_layout=channel_layout;
		enc->channels=channels;
		enc->sample_fmt=*(e->sample_fmts);
		enc->time_base={1,rate};
	}

	void alloc_enc()
	{
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

	void stage_open_input ()
	{
		ret=avformat_open_input (&fmtctx,
				filepath.c_str (),
				NULL, 
				NULL);
		if (ret<0) {
			throw ret;
		}
	}

	void stage_find_stream ()
	{
		ret=avformat_find_stream_info (fmtctx, NULL);
		if (ret<0) {
			throw ret;
		}
	}

	void stage_find_stream_index ()
	{
		ctx.stream_index=ret=av_find_best_stream (fmtctx,
				AVMEDIA_TYPE_AUDIO,-1,-1,&ctx.d,NULL);
		if (ret<0)
		{
			throw ret;
		}
	}


	void stage_alloc_context ()
	{
		ctx.dec_ctx = avcodec_alloc_context3 (ctx.d);
		if (!ctx.dec_ctx)
		{
			throw 1;
		}
		ret=avcodec_parameters_to_context(ctx.dec_ctx,
				fmtctx->streams[ctx.stream_index]->codecpar);
		if (ret<0)
		{
			throw ret;
		}
		ret=avcodec_open2 (ctx.dec_ctx, ctx.d, NULL);
		if (ret<0)
		{
			throw ret;
		}
		alloc_enc ();
	}

	void stage_alloc_swr ()
	{
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
		if (!swr)
		{

		}
		int ret=swr_init(swr);
		if (ret<0)
		{
			throw ret;
		}

	}

	Format (char *str) : filepath (str)
	{
	}

	public: 
	int error_eof{0};

	bool rightful_pkt ()
	{
		return ctx.stream_index==pkt->stream_index;
	}
	void stage_get_packet ()
	{
		int ret=0;
		if (error_eof)
		{
			throw error_eof;
		}
		ret=av_read_frame (fmtctx, pkt);
		if (ret==AVERROR_EOF)
		{
			error_eof=ret;
		}
		throw ret;
	}

	void stage_send_frame2decode()
	{
		ret=avcodec_send_packet (ctx.dec_ctx, pkt);
		if (ret<0)
		{
			throw ret;
		}
	}

	void stage_send_flush_decoder()
	{
		ret=avcodec_send_packet (ctx.dec_ctx, NULL);
		if (ret<0)
		{
			throw ret;
		}
	}

	void stage_receive_frame ()
	{
		if (!frame)
		{
			frame=av_frame_alloc ();
		}
		ret=avcodec_receive_frame (ctx.dec_ctx, frame);
		if (ret<0)
		{
			throw ret;
		}
	}

	int set_fframe=0;

	void stage_set_encoder_parameters ()
	{
		fframe=av_frame_alloc ();
		fframe->sample_rate=enc->sample_rate;
		fframe->channel_layout=enc->channel_layout;
		fframe->channels=enc->channels;
		fframe->format=enc->sample_fmt;
		av_frame_get_buffer (fframe, 0);
	}

	AVFrame* stage_get_frames()
	{
		int ret;

		ret=swr_convert_frame (swr, fframe, frame);
		av_frame_free (&frame);

		return fframe;
	}

	~Format ()
	{
		avcodec_free_context (&enc);
		avformat_close_input (&fmtctx);
		avformat_free_context(fmtctx);
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



};
struct gil
{
	PyGILState_STATE state;
	gil()
		: state(PyGILState_Ensure ())
	{
	}
	~gil(){
		PyGILState_Release (state);
	}
};


namespace filter {
	enum stage_t {
		STAGE_GET_SRC=0,
		STAGE_GET_SINK,
		STAGE_SEND_TO_ENCODER,
		STAGE_GET_PACKET,
		STAGE_NO_STAGE,
		STAGE_NO_OPT
	};

	struct fil_object {
		PyObject_HEAD
		filter_gh *fg;
		int stage;
	};
	using T=PyObject*;
	T fil_object_new (PyTypeObject* t, T a, T k)
	{
		return t->tp_alloc(t, 0);
	}
	int fil_object_init (T t, T a, T k)
	{
		fil_object* fb=(fil_object*)t;
		char *path;
		int num_of_outputs=1;
		int num_of_inputs=2;

		if (!PyArg_ParseTuple (a, "s|ii", &path,
					&num_of_inputs,
					&num_of_outputs
					))
			return 1;
		gil g;
		fb->fg=new filter_gh (num_of_inputs,
				num_of_outputs,
				path);
		fb->stage=STAGE_GET_SRC;
		return 0;
	}
	void fil_object_dealloc (T o)
	{
		fil_object* f=(fil_object*) o;
		delete f->fg;
	}
	T process_audio (T s, T a)
	{
		T arg;
		int index;
		int r;
		fil_object* f=(fil_object*) s;

		int stage=f->stage;
		filter_gh* fg=f->fg;

		if (!PyArg_ParseTuple (a, "Oi", &arg, &index))
			return NULL;

		if (Py_None!=arg)
		{

			AVFrame *frame=(AVFrame*)PyCapsule_GetPointer (arg, "_frame");
			fg->stage_get_src (index, frame);
			f->stage=STAGE_GET_SINK;
		}else
		{
			switch (stage)
			{
				case STAGE_GET_SINK:
					try {
						fg->stage_get_sink ();
						f->stage=STAGE_SEND_TO_ENCODER;
					}catch (...)
					{
						f->stage=STAGE_NO_STAGE;
					}
					break;
				case STAGE_SEND_TO_ENCODER:
					try {
						fg->stage_send_to_encoder ();
						f->stage=STAGE_GET_PACKET;
					}catch (...)
					{
						f->stage=STAGE_NO_STAGE;
					}
					break;
				case STAGE_GET_PACKET:
					try{
						AVPacket* pkt=fg->stage_get_packet ();
						if (pkt && pkt->size)
						{
							return PyBytes_FromStringAndSize ( (const char*)pkt->data,
									pkt->size);
							f->stage=STAGE_GET_PACKET;
						}
					}catch (...) {
						f->stage=STAGE_NO_STAGE;
					}
					break;
				case STAGE_NO_STAGE:
				default:
					f->stage=STAGE_GET_SRC;
					PyErr_Format (PyExc_StopIteration,
							"Stop Iteration");
					return NULL;
					break;
			}
		}

		Py_RETURN_NONE;
	}
	
	static PyMethodDef methods[]={
		{"process_audio",
			process_audio, METH_VARARGS,
			"send_frame_to_src"},
		{NULL, NULL, 0, NULL}
	};
	static PyTypeObject fobject_type ={
		PyVarObject_HEAD_INIT (NULL, 0)
		.tp_name="AVFilterGraph",
		.tp_init=fil_object_init,
		.tp_dealloc=fil_object_dealloc,
		.tp_new=fil_object_new,
		.tp_methods=methods,
		.tp_basicsize=sizeof(fil_object),
		.tp_flags=Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE
	};
};

namespace f
{
	enum stage_t {
		STAGE_NO_OPT=-53,
		STAGE_FATAL_ERROR,
		STAGE_ALLOC_ENCODER=0,
		STAGE_ENCODER_SET_PARAMETERS,
		STAGE_OPEN_INPUT,
		STAGE_FIND_STREAM,
		STAGE_FIND_STREAM_INDEX,
		STAGE_ALLOC_CONTEXT,
		STAGE_ALLOC_SWR,
		STAGE_GET_PACKET,
		STAGE_SEND_FRAME2DECODE,
		STAGE_SEND_FLUSH_DECODER,
		STAGE_RECEIVE_FRAME,
		STAGE_SET_ENCODER_PARAMETERS,
		STAGE_GET_FRAMES,
	};
	struct fobject {
		PyObject_HEAD
		Format* fmtctx;
		int stage;
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
		try{
			fb->fmtctx=new Format(path);
			fb->stage=STAGE_ALLOC_ENCODER;
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
	T process_audio (T s, T a)
	{
		fobject* f=(fobject*) s;
		Format *inner_f=f->fmtctx;
		int cur_stage=f->stage;

		switch (f->stage)
		{
			case STAGE_ALLOC_ENCODER:
				inner_f->stage_alloc_encoder();
				f->stage=STAGE_ENCODER_SET_PARAMETERS;
				break;
			case STAGE_ENCODER_SET_PARAMETERS:
				inner_f->stage_encoder_set_parameters();
				f->stage=STAGE_OPEN_INPUT;
				break;
			case STAGE_OPEN_INPUT:
				try{
					inner_f->stage_open_input();
					f->stage++;
				} catch (...)
				{
					f->stage=STAGE_FATAL_ERROR;
				}
				break;
			case STAGE_FIND_STREAM:
				try{
					inner_f->stage_find_stream();
					f->stage++;
				} catch (...)
				{
					f->stage=STAGE_FATAL_ERROR;
				}
				break;
			case STAGE_FIND_STREAM_INDEX:
				try{
					inner_f->stage_find_stream_index();
					f->stage++;
				} catch (...)
				{
					f->stage=STAGE_FATAL_ERROR;
				}
				break;
			case STAGE_ALLOC_CONTEXT:
				try {
					inner_f->stage_alloc_context();
					f->stage++;
				} catch (...)
				{
					f->stage=STAGE_FATAL_ERROR;
				}
				break;
			case STAGE_ALLOC_SWR:
				try {
					inner_f->stage_alloc_swr();
					f->stage++;
				} catch (...)
				{
					f->stage=STAGE_FATAL_ERROR;
				}
				break;
			case STAGE_GET_PACKET:
				try {
					inner_f->stage_get_packet();
				} catch (int& ret)
				{
					switch (ret)
					{
						case -1:
							f->stage=STAGE_FATAL_ERROR;
							break;
						case AVERROR_EOF:
							f->stage=STAGE_SEND_FLUSH_DECODER;
							break;
						default:
						{
							f->stage++;
							if (ret<0)
							{
								f->stage=STAGE_FATAL_ERROR;
							}else if (!inner_f->rightful_pkt ())
							{
								f->stage=STAGE_GET_PACKET;
							}
						}
					}
				}
				break;
			case STAGE_SEND_FRAME2DECODE:
				try {
					inner_f->stage_send_frame2decode();
					f->stage=STAGE_RECEIVE_FRAME;
				} catch (...)
				{
					f->stage=STAGE_GET_PACKET;
					f->stage=STAGE_RECEIVE_FRAME;
				}
				break;
			case STAGE_SEND_FLUSH_DECODER:
				try {
					inner_f->stage_send_flush_decoder();
					f->stage++;
				} catch (...)
				{
					f->stage=STAGE_FATAL_ERROR;
				}
				break;
			case STAGE_RECEIVE_FRAME:
				try {
					inner_f->stage_receive_frame();
					f->stage=STAGE_SET_ENCODER_PARAMETERS;
				} catch (int& err)
				{
					if (err==AVERROR_EOF)
					{
						f->stage=STAGE_FATAL_ERROR;
					}
					f->stage=STAGE_GET_PACKET;
				}
				break;
			case STAGE_SET_ENCODER_PARAMETERS:
				{
					inner_f->stage_set_encoder_parameters();
					f->stage=STAGE_GET_FRAMES;
				}
				break;
			case STAGE_GET_FRAMES:
				try {
					AVFrame* frame=NULL;
					f->stage=STAGE_RECEIVE_FRAME;

					frame=inner_f->stage_get_frames ();
					if (frame)
					{

						return PyCapsule_New (frame, "_frame",
					+[](T obj)
					{
						AVFrame* p=(AVFrame*)PyCapsule_GetPointer
						(obj, "_frame");
						av_frame_free(&p);
					});
					}
				} catch (int& ret)
				{
					cout<<"Error: "<<ret<<endl;
					if (inner_f->error_eof)
					{
						f->stage=STAGE_FATAL_ERROR;
					}
				}
				break;
			case STAGE_FATAL_ERROR:
				PyErr_Format (
					PyExc_RuntimeError,
					"Fatal Error: whilst trying to read this track."
						);
				return NULL;
			default:
				break;
		}
		struct stage_wth_name {
			stage_t stage;
			char* what;
		} stages[]=
		{
			{STAGE_FATAL_ERROR, "STAGE_FATAL_ERROR"},
			{STAGE_ALLOC_ENCODER, "STAGE_ALLOC_ENCODER"},
			{STAGE_ENCODER_SET_PARAMETERS, "STAGE_ENCODER_SET_PARAMETERS"},
			{STAGE_OPEN_INPUT, "STAGE_OPEN_INPUT"},
			{STAGE_FIND_STREAM, "STAGE_FIND_STREAM"},
			{STAGE_FIND_STREAM_INDEX, "STAGE_FIND_STREAM_INDEX"},
			{STAGE_ALLOC_CONTEXT, "STAGE_ALLOC_CONTEXT"},
			{STAGE_ALLOC_SWR, "STAGE_ALLOC_SWR"},
			{STAGE_GET_PACKET, "STAGE_GET_PACKET"},
			{STAGE_SEND_FRAME2DECODE, "STAGE_SEND_FRAME2DECODE"},
			{STAGE_SEND_FLUSH_DECODER, "STAGE_SEND_FLUSH_DECODER"},
			{STAGE_RECEIVE_FRAME, "STAGE_RECEIVE_FRAME"},
			{STAGE_SET_ENCODER_PARAMETERS, "STAGE_SET_ENCODER_PARAMETERS"},
			{STAGE_GET_FRAMES, "STAGE_GET_FRAMES"},
			{STAGE_NO_OPT, NULL}
		};
		char* cur_stage_s=NULL;
		char* stage_s=NULL;
		for (stage_wth_name *p=stages; p->what; p++)
		{
			if (p->stage==cur_stage)
			{
				cur_stage_s=p->what;
			}
			if (p->stage==f->stage)
			{
				stage_s=p->what;
			}
		}

		return PyTuple_Pack (2,
				PyTuple_Pack (2,
					Py_BuildValue ("s", cur_stage_s),
					PyLong_FromLong (cur_stage)
					),
				PyTuple_Pack (2,
					Py_BuildValue ("s", stage_s),
					PyLong_FromLong (f->stage)
					));

	}
	T get_frames (T s, T a)
	{
		T arg;
		if (!PyArg_ParseTuple (a, "O", &arg))
			return NULL;
		Py_RETURN_NONE;
	}
	T seek_duration (T s, T a)
	{
		int arg{0};
		if (!PyArg_ParseTuple (a, "|d", &arg))
			return NULL;
		try {
			fobject* f=(fobject*) s;
					gil g;
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
		{"process_audio",
			process_audio
			, METH_VARARGS,
		"process_audio: this method returns a tuple of (current_stage, next_stage) or a capsule of AVFrame* data;"},

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
		using t=PyObject*;

		PyObject *ov=PyModule_Create (&avv);

		PyType_Ready (&fobject_type);
		PyType_Ready (&filter::fobject_type);

		if (!Format_EOF)
		{
		Format_EOF=PyErr_NewException (
				"fobject.EOF",
				PyExc_Exception,
				PyDict_New ()
				);
		}
		Py_XINCREF (Format_EOF);
		PyModule_AddObject (ov, "EOF",
				Format_EOF);
		Py_XINCREF(&fobject_type);
		PyModule_AddObject (ov, "Format",
				(t)&fobject_type
		);
		Py_XINCREF(&filter::fobject_type);
		PyModule_AddObject (ov, "Filter",
				(T)&filter::fobject_type
		);

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
```
# res.py
```python

import fobject
import sys
import asyncio
import random
import aiofiles
import typing

class Pac(typing.List):
    def append(self, *args):
        if (len(self)>5000):
            del self[:90]
        else:
            super().append(*args)

async def run(*args):
        loop=asyncio.get_running_loop ()
        return await loop.run_in_executor(None, *args)

def track_threading (arg):
    for track in map (fobject.Format, sys.argv[1:]):
        while True:
            try:
                ret=track.process_audio ()
                if not isinstance(ret, tuple):
                    yield ret
            except RuntimeError:
                break

class Format(fobject.Format):
    def __init__(s, *a, **kw):
        s.args=a
        (fobject.Format).__init__(s, *a, *kw)
        pass
    async def __aiter__(self):
        while True:
            try:
                ret=self.process_audio ()
                if not isinstance(ret, tuple):
                    yield ret
            except RuntimeError:
                break

def args():
    yield from iter(sys.argv[1:])
    pass

async def rand(l):
    loop=asyncio.get_running_loop()
    async def randint(*x):
        return await loop.run_in_executor (None, random.randint, *x)
    s=l[await randint(0,len(l)-1)]
    return s

async def Deck(tracks):
    while True:
        x=Format(await rand(tracks))
        async for i in x:
            yield x, i

class Filter(fobject.Filter):
    pass

do_not_just_change=None
main_filter=Filter("[in1] lowpass, [in2]amerge, asetrate=44100*1.2[out]")

async def deck1(x, index):
    global do_not_just_change
    std=aiofiles.stdout.buffer
    async for x, i in Deck(x):
        await asyncio.sleep (0)
        main_filter.process_audio (i, index)
        while True:
            try:
                ret=main_filter.process_audio (None, index)
                if not ret:
                    continue
                sys.stdout.buffer.write (ret)
            except StopIteration:
                break


async def filter_switch():
    global main_filter
    global do_not_just_change
    i=0
    while True:
        await asyncio.sleep(19)
        if (i%2)==0:
            for j in range(100,300, 100):
                async with do_not_just_change:
                    main_filter=fobject.Filter(f"""[in1] lowpass,
                        [in2]amerge, asetrate=44100*1.{j}[out]""")
                await asyncio.sleep(0.8)
            main_filter=fobject.Filter(f"""[in2] anullsink;
                [in1]asetrate=44100*1.{j}[out]""")
            await asyncio.sleep(80)
            for j in range(100,300, -100):
                async with do_not_just_change:
                    main_filter=fobject.Filter(f"""[in1] lowpass,
                        [in2]amerge, asetrate=44100*1.{j}[out]""")
                await asyncio.sleep(0.8)
            main_filter=fobject.Filter(f"""[in2] anullsink;
                [in1]asetrate=44100*1.{j}[out]""")
        else:
            for j in range(100,300, 100):
                async with do_not_just_change:
                    main_filter=fobject.Filter(f"""[in2] lowpass,
                        [in1]amerge, asetrate=44100*1.{j}[out]""")
                await asyncio.sleep(0.8)
            main_filter=fobject.Filter(f"""[in1] anullsink;
                [in2]asetrate=44100*1.{j}[out]""")
            await asyncio.sleep(80)
            for j in range(100,300, -100):
                async with do_not_just_change:
                    main_filter=fobject.Filter(f"""[in2] lowpass,
                        [in1]amerge, asetrate=44100*1.{j}[out]""")
                await asyncio.sleep(0.8)
            main_filter=fobject.Filter(f"""[in1] anullsink;
                [in2]asetrate=44100*1.{j}[out]""")
        i=i+1

async def main (cb, *args):
    global do_not_just_change
    do_not_just_change=asyncio.Semaphore(2)
    await asyncio.gather(
        *map(lambda x: deck1(list(args), x), range(2)),
        filter_switch()
        )

asyncio.run (main(*args()))
```
