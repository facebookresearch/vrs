---
sidebar_position: 8
title: The FileHandler abstraction
---

*The VRS toolbox offers a powerful layer to abstract storage access: the `FileHandler` interface. It offers similar benefits as Python file-like objects, but goes a bit beyond by expanding the notion of “file path” to include URIs and json-format file specifications, and mechanisms to create the `FileHandler` that will be able to interpret them.*

While the `FileHandler` interface is part of the VRS toolbox, it is effectively a layer below the code that accesses VRS files. In practice, all IO accesses made by the VRS library will use `FileHandler` objects, but the `FileHandler` interface knows intrinsically nothing about VRS, or any storage for that matter.

## Why is `FileHandler` useful?

It quickly appeared that VRS files were not necessarily a single file on a hard drive. They could be:

* chunked files on one or more local hard drives
* a file in some cloud storage
* a list of binary blobs in cloud storage (chunks in cloud storage)

## What does `FileHandler` do?

The `FileHandler` interface primarily let’s you do all the read operations you would wish to do with a file:

* open the file, given a “path” (in effect, a text string to interpret, which will detail below),
* provide the file size,
* do `seek()` operations to move the read position,
* read a number of bytes,
* close the file & release allocated resources, if any (e.g., file system handles and/or caches).

At the core, that’s all `FileHandler` does.

## File Specifications, json File Specifications and URIs

The primary way to identify a file is to provide a file path, but quickly, that form proved inadequate. How do we represent a chunked file? How do we represent a file in cloud storage, a chunked file in cloud storage, or a file stored in Amazon's S3? How can we avoid to special case all these implementations in our APIs?

In order to keep our APIs simple, we needed to make it possible to always represent a file location as a string and provide a standardized way to point to methods that would allow VRS to interpret these file locations correctly and very differently. We settled on using json strings, with some required fields. Json file specifications strings are easy to recognize: they start and end with curly braces, which is a very cheap test, and then they can be parsed using a json parser, which should find a few required fields, some optional fields, and then some extra fields for some special use cases.

### Required Fields in json File Specifications

* `“storage"`: the name of the `FileHandler` able to read that location.
* `“chunks”`: an ordered list of strings, each representing a chunk of the logical file.

A valid json specification can simply be:  
`{"storage":"diskfile","chunks":["/local/folder/file.vrs"]}`  

A chunked local file looks like so:  
`{"storage":"diskfile","chunks":["/local/folder/file.vrs","/local/folder/file.vrs_chunk2"]}`  

A chunked file in Everstore might be accessed using chunks published as HTTP objects, and be represented like so:  
`{"storage":"http","chunks":["http://cdn.facebook.com/SOME-HASH","http://cdn.facebook.com/OTHER-HASH"]}`

### Optional Fields

When dealing with objects in cloud storage, it can be expansive to do some basic-looking operations such as getting a file chunk’s size, in particular when you have many of them, knowing how to name the file if we want to download and save it locally, or how to reference the file. The following optional fields have been introduced to answer these questions:

* `“chunk_sizes”`: an ordered list of integers, which should match the list of `“chunks”`.
* `“filename”`: a name suitable for the object as a local file. It might be the name of the file before it was uploaded to cloud storage.
* `“source_uri”`: a URI representation of where the object comes from, in particular if the file specification was generated from a URI.

### Extra fields

Sometimes, more fields can be provided, outside of the json specifications. These might include additional authentication options for some cloud storage implementations.

### FileSpec

Parsing json messages can be expensive, in particular if we need to repeat the parsing multiple times. This is why internally, string paths are always immediately converted into `FileSpec` objects, which are used for all file location operations. At its core, a `FileSpec` object is simply a struct with the following public fields:

```
  string fileHandlerName;
  string fileName;
  string uri;
  vector<string> chunks;
  vector<int64_t> chunkSizes;
  map<string, string> extras;
```

`FileSpec` offers a few helper functions to convert to and from json, get the file size (if specified directly), etc. In practice, json file specifications and `FileSpec` objects are equivalent in meaning, and map 1:1.

### URIs

A common way to represent files is the URI specification. We did not standardize on URIs, because chunked local files, which was the first use case for json specifications, did not work well in that format. However, URIs can be very convenient, so we made URIs work with the `FileHandler` system, by combining URI parsing, `FileHandler` objects, and the next key building block: `FileHandlerFactory`.

## FileHandlerFactory

The primary role of `FileHandler` is to give an abstract way to read files in each of these forms. But except for local files, all the other `FileHandler` implementations do not belong in VRS core, which is limited to a small set of library dependencies, so that it can easily be compiled for mobile or even embedded environments.
The `FileHandlerFactory` singleton allows for:

* registering custom `FileHandler` implementations,
* requesting the construction of `FileHandler` objects, by name,
* opening a file specification using the appropriate `FileHandler`.

`FileHandlerFactory` is a very small but essential class. Any time a file needs to be open in VRS, `FileHandlerFactory` will be used to (basically):

* convert the string path into a `FileSpec`,
* create a `FileHandler` instance specified by name in the `FileSpec` object,
* ask a `FileHandler` instance to actually “open” the `FileSpec`, whatever that might mean for that `FileHandler`,
* return an error code and the `FileHandler` object (if any).

All this is done by the API:

```
int FileHandlerFactory::delegateOpen(
  const string& path,
  unique_ptr<FileHandler>& outNewDelegate);
```

When VRS’ `RecordFileReader` class opens a VRS file using a string path, this is the API used to get the `FileHandler` needed for all read operations, and it will never know if the file is local, chunked, or in some form of cloud storage.

### Further Nested Delegation

For completeness, there is one more level of indirection required. When `FileHandlerFactory` asks a `FileHandler` to open a file, it uses `FileHandler`'s eponym API to `delegateOpen` the `FileSpec`, so that the `FileHandler` itself may delegate the actual handling of the `FileSpec` to yet another `FileHandler` implementation. 

For instance, imagine your have an "http" `FileHandler`, which can stream data from an http file.
Also imagine that you have a database named "aria" of files in cloud storage, which references files using a 64 bit number as identified. You could implement the "aria" `FileHandler`, that would allow you to convert aria URIs into URLs, by accessing the aria database. So the aria `FileHandler` would convert the URI "aria:12345", for the dataset #12345 in a CDN URL located in a server near your machine. The aria `FileHandler` can then delegate reading the data from that URL to the "http" `FileHandler`.


## Interpreting strings as json specifications, URIs, or file paths

Putting it all together, when VRS needs to interpret a string path, the following logic is used:

* If string path is a json specification, it is converted into a `FileSpec` object, which includes, hopefully, a `FileHandler` name.
* If the string path looks like a URI, a `FileHandler` with the name of the URI scheme, if there is one available, will be asked to parse the rest of the URI to make a valid `FileSpec` object.
* Otherwise, the string path will be assumed to be a local file path, that VRS' built-in “diskfile” `FileHandler` will be able to read using standard POSIX file APIs.
* Once we have a `FileSpec` object, we can use `FileHandlerFactory` to `delegateOpen()` the file.

## FileHandler vs. WriteFileHandler

As you may have noted, `FileHandler` is strictly a read-only interface, because most `FileHandler` implementations, by far, only support reading. This is largely because cloud storage is usually either completely immutable, or it offers very limited modification options, and write operations are very constrained (append and/or concat). The `WriteFileHandler` that derives from `FileHandler` adds support for write operations.

But don’t be fooled: `WriteFileHandler` is a much trickier business.

### Reading is easy

A (read) `FileHandler` only really needs to implement the following operations:

* `open()` & `close()`
* `getFileSize()`
* `seekTo(position)`
* `readBytes(length)`

Effectively, cloud storage is typically stateless, and only offers `getFileSize()` and `readRange(position, length)`, but these are enough to implement the `FileHandler` interface. As a result, it’s easy to read cloud stored data using the same interface as for files, even if performance will be obviously differ greatly. In practice, all our `FileHandler` interfaces fully implement the read operations, or delegate to other `FileHandler` implementations that will.

### Writing is “different”

When you write a file with typical disk file APIs, you can:

* create a file at a location, and immediately, an entry appears in the file system,
* write bytes in it, and extend the file,
* write more bytes in it, and extend the file further,
* handle write requests of any size, because OS system caches will smooth out performance issues for you,
* seek to a past location, to read from there what you’ve written, and/or overwrite what’s there with a new write operation,
* overwrite or extend a file, but you can’t insert bytes in a middle of the file,
* of course, you can also open an existing file to modify it,
* if your app crashes and/or doesn’t close your file explicitly, depending on implementation, data may or may not be fully written out to disk, and it’s easy to have a partially written file.

In contrast, when writing files to a cloud storage, things vary a lot:

* files typically aren’t created until a final close/commit/submit/finalize operation is performed after a series of successful write operations,
* writes are network operations, so have very high latencies,
* small writes can be extremely inefficient on the backend in terms of storage, so writing data in large chunks makes a huge difference.
* typically, you can not read back data you’ve just written, until you’ve finalized the object and it's fully "committed".
* typically, you can not overwrite/modify data you’ve already written,
* typically, if an app crashes before the upload is finalized, anything uploaded is lost, though sometimes, it’s possible to recover partial uploads or chunks.

In practice, all these constrains vary greatly between cloud storage solutions. If “all” you need to do is upload an existing file “as is” to a cloud storage, that’s probably reasonably simple, though not necessarily trivial either. For instance, cloud storage solutions may have a maximum file size, or some chunking preference, so if you want to store large files, you’re going to need to chunk them and manage the chunks yourself to a certain extent.

### Don’t use `WriteFileHandler` implementations directly

Because cloud storage write operations just don’t allow the type of flexibility provided by a typical file API, it’s probably impossible to provide a generic yet effective `WriteFileHandler` implementation that will work reasonably well for all use cases. So cloud implementations of `WriteFileHandler` are just not designed to do that. While any `FileHandler` implementation can safely be used to access any type of file anywhere they are, you can not think of `WriteFileHandler` the same way.
