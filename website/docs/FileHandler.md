---
sidebar_position: 8
title: The FileHandler Interface
---

The VRS toolbox offers a powerful layer to abstract storage access: the `FileHandler` interface.

`FileHandler` offers benefits similar to those of Python file-like objects, but goes beyond that by expanding the scope of “file path” to include URIs and JSON-format file specifications. It also provides mechanisms to create a `FileHandler` that will be able to interpret them.

VRS uses `FileHandler` to access recording data, but the interface itself is not specific to the VRS file format or to any storage backend.

## Why is `FileHandler` useful?

VRS files are not necessarily a single file on a hard drive. They can be any of the following types of files:

- Chunked files on one or more local hard drives
- A binary blob in cloud storage
- A list of binary blobs in cloud storage (chunks in cloud storage)
- Some kind of database reference that points to any of the above...

## What does `FileHandler` do?

The `FileHandler` interface enables you to do the following read operations with a file:

- Open the file with a given path (a text string to interpret, details are below)
- Provide the file size
- Move the read position
- Read a number of bytes
- Provide caching hints
- Close the file and release the allocated resources (such as file system handles and caches)

At the core, that is all `FileHandler` does.

## File Paths, URIs, and JSON File Specifications

The primary way to identify a file is to provide a file path, but that is insufficient.

We need to be able to handle the following use cases:

- Chunked files
- Files in cloud storage
- Chunked files in cloud storage
- References to a database entry, pointing to a file in another storage location
- Files stored in Amazon AWS S3, Microsoft Azure, Google Cloud Storage, etc.

We want to avoid making special-case implementations of our APIs and keep them simple, so using a single string to represent any path is a requirement. URIs are powerful and convenient, but chunked files quickly become difficult to represent, as chunked files can be a collection of URIs. For extreme cases, JSON appears to be the only reasonable option.

In the end, VRS supports regular file paths, URIs, and JSON file specifications, as transparently as possible.

### JSON File Specifications

A JSON file specification can identify a handler and list the chunks to open:

- `storage`: the name of the `FileHandler` that can read that location. If omitted, VRS normally uses `diskfile`.
- `chunks`: an ordered list of strings, each representing a chunk of the logical file, in a format that the designated `FileHandler` can handle.

A valid JSON file specification can simply be as follows:  
`{"storage":"diskfile","chunks":["/local/folder/file.vrs"]}`

A chunked local file may look like this:  
`{"storage":"diskfile","chunks":["/local/folder/file.vrs","/local/folder/file.vrs_part_2"]}`

A chunked file in the cloud might use a specification like this, if an HTTP `FileHandler` has been registered:  
`{"storage":"http","chunks":["https://example.com/HASH1","https://example.com/HASH2"]}`

**Optional Fields**

When dealing with objects in cloud storage, even getting the size of a chunk can require a network request. These optional fields can provide chunk sizes and preserve the original file name or URI:

- `chunk_sizes`: an ordered list of integer byte sizes, one per entry in `chunks`.
- `filename`: a name suitable for saving the file locally. It might be the name of the file before it was uploaded to cloud storage.
- `source_uri`: the URI originally used to identify the file, if applicable.

Example:  
`{"storage":"http","chunks":["https://example.com/HASH1"],"chunk_sizes":[123456],"filename":"scene23.vrs","source_uri":"dataset:456789"}`

**Extra fields**

Additional fields use string values and names not reserved by the file specification. A handler can use them for storage-specific options, for example:  
`{"storage":"http","chunks":["https://example.com/HASH1"],"source_uri":"dataset:456789","cache":"local"}`

### URIs

VRS uses the URI scheme to select a registered `FileHandler` or delegator. That implementation can define how to interpret the rest of the URI.  
Example: `dataset:456789?cache=local`

### `FileSpec`

VRS converts a path, URI, or JSON specification into a `FileSpec` before opening the file.

At its core, a `FileSpec` object is simply a struct with the following public fields:

```
  string fileHandlerName;
  string fileName;
  string uri;
  vector<string> chunks;
  vector<int64_t> chunkSizes;
  Extras extras;
```

`FileSpec` provides helper functions to convert to and from JSON and to get the size of a file (when specified directly). In practice, JSON file specifications and `FileSpec` objects are equivalent.

## `FileHandlerFactory`

The primary role of `FileHandler` is to provide a common interface for reading files. VRS core provides `diskfile` for local files and, on supported platforms, `asyncdiskfile`. Access to other storage backends requires a separately registered implementation, keeping the core library's dependencies small.

The `FileHandlerFactory` singleton allows:

- Registering additional `FileHandler` implementations
- Requesting the construction of `FileHandler` objects by name
- Interpreting a file represented by a local file path, a URI, or a JSON file specification using the appropriate `FileHandler`.

`FileHandlerFactory` is a pretty small but essential class, used to open files as follows:

- Convert a string path into a `FileSpec`
- Create a `FileHandler` instance specified by name in the `FileSpec` object
- Request a `FileHandler` instance to actually open the `FileSpec`, whatever that means for that `FileHandler`
- Return an error code and the `FileHandler` object

All this is done by the `FileHandlerFactory` API as shown below:

```
int FileHandlerFactory::delegateOpen(
  const string& path,
  unique_ptr<FileHandler>& outNewDelegate);
```

When the VRS `RecordFileReader` class opens a VRS file using the string path, the `FileHandlerFactory` API finds the `FileHandler` that is needed for all read operations. The `RecordFileReader` object does not need to know if the VRS file is local, chunked, or in cloud storage.

### Further Nested Delegation

A `FileHandler` can open the `FileSpec` itself or delegate the work to another handler. Each delegation uses `delegateOpen()`, so the reader does not need to know which handler ultimately opens the file.

For example, imagine you have an HTTP `FileHandler`, which can stream data from an HTTP file. Also imagine you have a database of VRS files in cloud storage, named "MyAIDataSet", which references files using a 64 bit number. You can implement a custom `MyAIDataSetFileHandler`, which accesses the MyAIDataSet database, and converts MyAIDataSet URIs into HTTP URLs. The `MyAIDataSetFileHandler` code will convert the URI, "myaidataset:12345", for the MyAIDataSet dataset #12345, into a content delivery network (CDN) URL, appropriate for your computer (it might do that by looking up the dataset online using a service that will return that URL after validations and permissions checks). Then, the `MyAIDataSetFileHandler` logic will delegate reading the data from that URL to the HTTP `FileHandler` that can stream the data from an HTTP URL. The `RecordFileReader` will decode the remote VRS file just as if it were a local file, because all its file accesses are done using a `FileHandler` instance.

## Interpreting Strings as JSON File Specifications, URIs, or File Paths

Putting it all together, when VRS needs to interpret a string path, the following logic is used:

- If the string is a JSON specification, it is converted into a `FileSpec`.
- If the string is a URI, the registered handler or delegator for that scheme can parse it. Otherwise, VRS uses its generic URI parser; a successful parse does not mean the scheme can be opened.
- Otherwise, the string is treated as a local path by default.
- Once the `FileSpec` is built, `FileHandlerFactory::delegateOpen()` selects a handler or delegator and tries to open the file.

## `FileHandler` vs. `WriteFileHandler`

As you may have noticed, `FileHandler` is strictly a read-only interface, because most `FileHandler` implementations, by far, only support reading. This is largely because cloud storage is usually either completely immutable, or only offers very limited creation and mutation options. Some cloud storage might only support creating new objects. Other cloud storage might only support creating new objects of constrained size, but will be able to concatenate existing objects to create new objects.

`WriteFileHandler` extends `FileHandler` with write operations. Not every implementation supports overwriting data or reopening an existing file for updates.

### Reading is easy

The core read operations are:

- `openSpec()`
- `close()`
- `getTotalSize()`
- `setPos(offset)`
- `read(buffer, length)`

A cloud storage service typically provides object sizes and byte-range reads. A `FileHandler` can use these to implement the same read operations as for local files, and add caching where useful.

### Writing is complex

You can do the following file operations with typical disk file APIs:

- Create a file at a location, so that an entry appears in the file system, immediately.
- Write bytes to the file and extend the file.
- Write more bytes to the file, and extend the file further.
- Handle write requests of any size (OS system caching minimizes performance issues for you).
- Seek to a past location, to read what is written before.
- Overwrite or extend a file. (You cannot insert bytes into the middle of a file.)
- Open an existing file to modify it.
- If your app crashes or does not close the file explicitly, the data may not be fully written to disk, depending on the implementation, so it is common to have partially written files.

In contrast, when writing files to a cloud storage, things typically happen as follows:

- Files are not actually created until the final close/commit/submit/finalize operation is performed, after a series of successful write operations.
- Write operations are network operations. They have very high latencies and high error rates. Retrying file write operations is often required.
- Small write operations can be extremely inefficient on the backend, in terms of storage. So, writing data in large chunks makes a huge difference.
- You can not read back data you have just written, until you have finalized the object, and it is fully committed.
- You can not overwrite or modify data you have already written.
- If an app crashes before the upload is finalized, anything uploaded is probably lost. Some cloud storage solutions can recover partial uploads or chunks, but at the cost of added code complexity.

In practice, all these constraints vary greatly between cloud storage solutions. Even if you only need to upload existing files, as they are, to a cloud storage, the cloud storage may have a maximum file size, or a chunking preference. So, if you want to store large files in the cloud, you have to chunk them and manage the chunks yourself to a certain extent.

### Do not use `WriteFileHandler` implementations directly

Cloud storage write operations do not provide the same type of flexibility as file APIs. Consequently, it is usually not possible to provide a generic `WriteFileHandler` implementation - one that will work for all use cases. While any `FileHandler` implementation can safely be used to access any type of file anywhere, you cannot think of `WriteFileHandler` as something that will work for all use cases.
