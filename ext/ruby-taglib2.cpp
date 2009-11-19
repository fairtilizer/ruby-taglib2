#include <attachedpictureframe.h>
#include <fileref.h>
#include <flacfile.h>
#include <id3v2frame.h>
#include <id3v2tag.h>
#include <mpegfile.h>
#include <tag.h>
#include <vorbisfile.h>

// Avoid redefinition warning on Mac OS
#ifdef __APPLE__
#undef alloca
#endif

extern "C"
{
#include "ruby.h"
}

////////////////////////
// Storage for module TagLib2 and its classes
////////////////////////
static VALUE TagLib2Module;
static VALUE FileClass;
static VALUE RefClass;
static VALUE ImageClass;
static VALUE BadFile;
static VALUE BadTag;
static VALUE BadAudioProperties;

////////////////////////
// Support functions useful repeatedly for bridging TagLib strings to Ruby strings
////////////////////////
static VALUE TStrToRubyStr(const TagLib::String str)
{
	const char *s = str.toCString(true);
	return rb_str_new(s, strlen(s));
}

static TagLib::String RubyStrToTStr(VALUE s)
{
	VALUE rStr = StringValue(s);
	char *cStr = new char[RSTRING_LEN(rStr) + 1];
	cStr[RSTRING_LEN(rStr)] = 0;
	memcpy(cStr, RSTRING_PTR(rStr), RSTRING_LEN(rStr));
	TagLib::String string(cStr, TagLib::String::UTF8);
	delete[] cStr;
	return string;
}

////////////////////////
// Class to integrate Mahoro type detection into TagLib
////////////////////////
class MahoroFileTypeResolver : public TagLib::FileRef::FileTypeResolver
{
public:
	virtual TagLib::File *createFile(const char *, bool, TagLib::AudioProperties::ReadStyle) const;
};

TagLib::File *MahoroFileTypeResolver::createFile(const char *fileName, bool read,
                                   TagLib::AudioProperties::ReadStyle style) const
{
	if(RTEST(rb_const_get(TagLib2Module, rb_intern("MAHORO_PRESENT"))))
	{
		VALUE Mahoro = rb_const_get(rb_cObject, rb_intern("Mahoro"));
		VALUE mahoro = rb_class_new_instance(0, 0, Mahoro);
		rb_funcall(mahoro, rb_intern("flags="), 1,
		           rb_const_get(Mahoro, rb_intern("NONE")));

		VALUE mime = rb_funcall(mahoro, rb_intern("file"), 1,
		                        rb_str_new(fileName, strlen(fileName)));

		if(RTEST(rb_funcall(mime, rb_intern("include?"), 1, rb_str_new("MP3", 3))))
		{
			return new TagLib::MPEG::File(fileName, read, style);
		}

		if(RTEST(rb_funcall(mime, rb_intern("include?"), 1, rb_str_new("Ogg", 3))) ||
		   RTEST(rb_funcall(mime, rb_intern("include?"), 1, rb_str_new("ogg", 3))) )
		{
			if(RTEST(rb_funcall(mime, rb_intern("include?"), 1, rb_str_new("Vorbis", 6))) ||
			   RTEST(rb_funcall(mime, rb_intern("include?"), 1, rb_str_new("vorbis", 6))) )
			{
				return new TagLib::Vorbis::File(fileName, read, style);
			}
			if(RTEST(rb_funcall(mime, rb_intern("include?"), 1, rb_str_new("FLAC", 6))) )
			{
				return new TagLib::FLAC::File(fileName, read, style);
			}
			
		}
	}
	return 0;
}

////////////////////////
// Block that requires Mahoro, which we need to be able to eat the exception when
// Mahoro is not installed
////////////////////////
static VALUE requireMahoro(VALUE)
{
	return rb_require("mahoro");
}

////////////////////////
// Deallocate class TagLib2::File::FileRef
////////////////////////
static void freeRef(void *ref)
{
	TagLib::FileRef *f = static_cast<TagLib::FileRef *>(ref);
	delete f;
}

////////////////////////
// Initialize class TagLib2::File
////////////////////////
static VALUE FileInitialize(VALUE self, VALUE path)
{
	rb_iv_set(self, "@path", path);
	TagLib::FileRef *f = new TagLib::FileRef(RSTRING_PTR(StringValue(path)));

	if(!f || f->isNull())
	{
		rb_raise(BadFile, "Unable to open file with TagLib");
		return Qnil;
	}

	VALUE ref = Data_Wrap_Struct(RefClass, 0, freeRef, f);
	rb_iv_set(self, "@ref", ref);

	return self;
}

////////////////////////
// Macros to collapse code common to most of the TagLib2::File accessors
////////////////////////
#define GetFileRef \
	TagLib::FileRef *f; \
	Data_Get_Struct(rb_iv_get(self, "@ref"), TagLib::FileRef, f);

#define GetID3Tag \
	TagLib::MPEG::File *mpegFile = dynamic_cast<TagLib::MPEG::File *>(f->file()); \
	if(!mpegFile) \
		return Qnil; \
\
	TagLib::ID3v2::Tag *tag = mpegFile->ID3v2Tag(); \
	if(!tag) \
		return Qnil;

#define CheckTag(f) \
	if(!f->tag()) \
	{ \
		rb_raise(BadTag, "TagLib produced an invalid tag"); \
		return Qnil; \
	}

#define CheckProperties(f) \
	if(!f->audioProperties()) \
	{ \
		rb_raise(BadAudioProperties, "TagLib produced invalid audio properties"); \
		return Qnil; \
	}


#define StringAttribute(name, capName) \
static VALUE FileRead##capName(VALUE self) \
{ \
	GetFileRef \
	CheckTag(f) \
	return TStrToRubyStr(f->tag()->name()); \
} \
static VALUE FileWrite##capName(VALUE self, VALUE arg) \
{ \
	GetFileRef \
	CheckTag(f) \
	f->tag()->set##capName(RubyStrToTStr(arg)); \
	return arg; \
}

#define UIntAttribute(name, capName) \
static VALUE FileRead##capName(VALUE self) \
{ \
	GetFileRef \
	CheckTag(f) \
	return UINT2NUM(f->tag()->name()); \
} \
static VALUE FileWrite##capName(VALUE self, VALUE arg) \
{ \
	GetFileRef \
	CheckTag(f) \
	f->tag()->set##capName(NUM2UINT(arg)); \
	return arg; \
}

#define AttributeMethods(name, capName) \
	rb_define_method(FileClass, #name, (VALUE (*)(...))FileRead##capName, 0); \
	rb_define_method(FileClass, #name "=", (VALUE (*)(...))FileWrite##capName, 1);

#define IntProperty(name, capName) \
static VALUE FileRead##capName(VALUE self) \
{ \
	GetFileRef \
	CheckProperties(f) \
	return UINT2NUM(f->audioProperties()->name()); \
}

#define PropertyMethod(name, capName) \
	rb_define_method(FileClass, #name, (VALUE (*)(...))FileRead##capName, 0); \

////////////////////////
// TagLib2::File accessors
////////////////////////

StringAttribute(title, Title)
StringAttribute(artist, Artist)
StringAttribute(album, Album)
StringAttribute(comment, Comment)
StringAttribute(genre, Genre)
UIntAttribute(year, Year)
UIntAttribute(track, Track)
IntProperty(bitrate, Bitrate)
IntProperty(sampleRate, SampleRate)
IntProperty(channels, Channels)
IntProperty(length, Length)

////////////////////////
// TagLib2::File#save
////////////////////////
static VALUE FileSave(VALUE self)
{
	GetFileRef

	f->save();
}

////////////////////////
// TagLib2::FileSave#eachImage
////////////////////////
static VALUE FileEachImage(VALUE self)
{
	GetFileRef
	GetID3Tag

	VALUE apic = rb_str_new("APIC", 4);
	for(TagLib::ID3v2::FrameList::ConstIterator i = tag->frameList().begin();
	    i != tag->frameList().end(); ++i)
	{
		TagLib::ID3v2::AttachedPictureFrame *f = dynamic_cast<TagLib::ID3v2::AttachedPictureFrame *>(*i);
		if(f)
		{
			VALUE id = rb_str_new((const char *)f->frameID().data(), f->frameID().size());
			if(RTEST(rb_funcall(id, rb_intern("=="), 1, apic)))
			{
				VALUE image = Data_Wrap_Struct(ImageClass, 0, 0, f);
				rb_yield(image);
			}
		}
	}
	return self;
}

////////////////////////
// Wrapper around FileEachImage for use with rb_iterate
////////////////////////
static VALUE callEachImage(VALUE obj)
{
	return rb_funcall(obj, rb_intern("eachImage"), 0, 0);
}

////////////////////////
// Support block for FileImageCount
////////////////////////
static VALUE counter(VALUE image, VALUE *count)
{
	return (*count) = rb_funcall(*count, rb_intern("+"), 1, UINT2NUM(1));
}

////////////////////////
// TagLib2::File#imageCount
////////////////////////
static VALUE FileImageCount(VALUE self)
{
	VALUE count = UINT2NUM(0);
	rb_iterate(callEachImage, self, (VALUE (*)(...))counter, (VALUE)&count);
	return count;
}

////////////////////////
// Support block for FileImage
////////////////////////
static VALUE imageFinder(VALUE image, VALUE ary)
{
	VALUE count = rb_ary_entry(ary, 0);
	VALUE idx = rb_ary_entry(ary, 1);
	if(count == idx)
	{
		rb_ary_store(ary, 2, image);
		rb_iter_break();
	}
	rb_ary_store(ary, 0, rb_funcall(count, rb_intern("+"), 1, UINT2NUM(1)));
}

////////////////////////
// TagLib2::File#image
////////////////////////
static VALUE FileImage(VALUE self, VALUE idx)
{
	VALUE ary = rb_ary_new3(2, UINT2NUM(0), idx);
	rb_iterate(callEachImage, self, (VALUE (*)(...))imageFinder, ary);
	return rb_ary_entry(ary, 2);
}

////////////////////////
// TagLib2::File#addImage
////////////////////////
static VALUE FileAddImage(VALUE self, VALUE type, VALUE mimeType,
                          VALUE description, VALUE data)
{
	GetFileRef
	GetID3Tag
	TagLib::ID3v2::AttachedPictureFrame *frame = new TagLib::ID3v2::AttachedPictureFrame;
	frame->setTextEncoding(TagLib::String::UTF8);

	if(RTEST(type))
	{
		frame->setType((TagLib::ID3v2::AttachedPictureFrame::Type)NUM2UINT(type));
	}
	else
	{
		delete frame;
		rb_raise(rb_eArgError, "Type must be specified");
	}

	if(RTEST(mimeType))
	{
		frame->setMimeType(RubyStrToTStr(StringValue(mimeType)));
	}
	else
	{
		delete frame;
		rb_raise(rb_eArgError, "mimeType must be specified");
	}

	if(RTEST(description))
	{
		frame->setDescription(RubyStrToTStr(StringValue(description)));
	}

	if(RTEST(data))
	{
		VALUE str = StringValue(data);
		frame->setPicture(TagLib::ByteVector(RSTRING_PTR(data), RSTRING_LEN(data)));
	}
	tag->addFrame(frame);
}

////////////////////////
// TagLib2::File#removeImage
////////////////////////
static VALUE FileRemoveImage(VALUE self, VALUE image)
{
	GetFileRef
	GetID3Tag
	TagLib::ID3v2::AttachedPictureFrame *frame;
	Data_Get_Struct(image, TagLib::ID3v2::AttachedPictureFrame, frame);
	tag->removeFrame(frame, true);
}

////////////////////////
// TagLib2::Image#type
////////////////////////
static VALUE ImageType(VALUE self)
{
	TagLib::ID3v2::AttachedPictureFrame *frame;
	Data_Get_Struct(self, TagLib::ID3v2::AttachedPictureFrame, frame);
	return UINT2NUM(frame->type());
}

////////////////////////
// TagLib2::Image#description
////////////////////////
static VALUE ImageDescription(VALUE self)
{
	TagLib::ID3v2::AttachedPictureFrame *frame;
	Data_Get_Struct(self, TagLib::ID3v2::AttachedPictureFrame, frame);
	return TStrToRubyStr(frame->description());
}

////////////////////////
// TagLib2::Image#mimeType
////////////////////////
static VALUE ImageMimeType(VALUE self)
{
	TagLib::ID3v2::AttachedPictureFrame *frame;
	Data_Get_Struct(self, TagLib::ID3v2::AttachedPictureFrame, frame);
	return TStrToRubyStr(frame->mimeType());
}

////////////////////////
// TagLib2::Image#data
////////////////////////
static VALUE ImageData(VALUE self)
{
	TagLib::ID3v2::AttachedPictureFrame *frame;
	Data_Get_Struct(self, TagLib::ID3v2::AttachedPictureFrame, frame);
	return rb_str_new(frame->picture().data(), frame->picture().size());
}

////////////////////////
// TagLib2::Image#id
////////////////////////
static VALUE ImageID(VALUE self)
{
	TagLib::ID3v2::AttachedPictureFrame *frame;
	Data_Get_Struct(self, TagLib::ID3v2::AttachedPictureFrame, frame);
	return UINT2NUM(frame->frameID().toUInt());
}


////////////////////////
// Main function to create module TagLib2 and its classes
////////////////////////
extern "C" void Init_taglib2(void)
{
	TagLib2Module = rb_define_module("TagLib2");

	BadFile = rb_define_class_under(TagLib2Module, "BadFile", rb_eRuntimeError);
	ImageClass = rb_define_class_under(TagLib2Module, "Image", rb_cObject);
	FileClass = rb_define_class_under(TagLib2Module, "File", rb_cObject);
	RefClass = rb_define_class_under(FileClass, "FileRef", rb_cObject);
	rb_define_method(FileClass, "initialize", (VALUE (*)(...))FileInitialize, 1);
	AttributeMethods(title, Title)
	AttributeMethods(artist, Artist)
	AttributeMethods(album, Album)
	AttributeMethods(comment, Comment)
	AttributeMethods(genre, Genre)
	AttributeMethods(year, Year)
	AttributeMethods(track, Track)
	PropertyMethod(bitrate, Bitrate)
	PropertyMethod(sampleRate, SampleRate)
	PropertyMethod(sample_rate, SampleRate)
	PropertyMethod(channels, Channels)
	PropertyMethod(length, Length)
	rb_define_method(FileClass, "save", (VALUE (*)(...))FileSave, 0);
	rb_define_method(FileClass, "eachImage", (VALUE (*)(...))FileEachImage, 0);
	rb_define_method(FileClass, "each_image", (VALUE (*)(...))FileEachImage, 0);
	rb_define_method(FileClass, "imageCount", (VALUE (*)(...))FileImageCount, 0);
	rb_define_method(FileClass, "image_count", (VALUE (*)(...))FileImageCount, 0);
	rb_define_method(FileClass, "image", (VALUE (*)(...))FileImage, 1);
	rb_define_method(FileClass, "addImage", (VALUE (*)(...))FileAddImage, 4);
	rb_define_method(FileClass, "add_image", (VALUE (*)(...))FileAddImage, 4);
	rb_define_method(FileClass, "removeImage", (VALUE (*)(...))FileRemoveImage, 1);
	rb_define_method(FileClass, "remove_image", (VALUE (*)(...))FileRemoveImage, 1);
	rb_define_method(ImageClass, "type", (VALUE (*)(...))ImageType, 0);
	rb_define_method(ImageClass, "description", (VALUE (*)(...))ImageDescription, 0);
	rb_define_method(ImageClass, "mimeType", (VALUE (*)(...))ImageMimeType, 0);
	rb_define_method(ImageClass, "mime_type", (VALUE (*)(...))ImageMimeType, 0);
	rb_define_method(ImageClass, "data", (VALUE (*)(...))ImageData, 0);
	rb_define_method(ImageClass, "id", (VALUE (*)(...))ImageID, 0);

	// Load Mahoro if available
	int failed = 0;
	VALUE ret = rb_protect(requireMahoro, Qnil, &failed);

	rb_define_const(TagLib2Module, "MAHORO_PRESENT", failed ? Qfalse : Qtrue);
}
