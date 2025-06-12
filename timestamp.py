import vrs

def adjust_timestamps(input_path, output_path, time_offset_sec):
    # Open original file for reading
    reader = vrs.RecordFileReader()
    reader.openFile(input_path, skipChecks=True)

    # Create a writer for output
    writer = vrs.RecordFileWriter()
    writer.setFile(output_path)

    # Set metadata tags
    for key, val in reader.getTags().items():
        writer.setTag(key, val)

    # Add all streams
    for info in reader.getStreamInfos():
        writer.addStream(info.streamId, info)

    # Convert time_offset to nanoseconds
    offset_ns = int(time_offset_sec * 1e9)

    # Iterate through all records
    for record in reader:
        header = record.header
        new_header = vrs.RecordHeader(
            streamId=header.streamId,
            recordType=header.recordType,
            timestamp=header.timestamp + offset_ns,
            format=header.format
        )
        # Copy the record data
        new_record = vrs.Record(
            header=new_header,
            data=record.data,
            elementCount=record.elementCount
        )
        writer.writeRecord(new_record)

    writer.close()
    print(f"Saved shifted VRS file to: {output_path}")

# Usage Example
adjust_timestamps("input.vrs", "output.vrs", time_offset_sec=1.0)
