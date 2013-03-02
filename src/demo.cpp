#include "demo.h"

#include <snappy.h>

#include "debug.h"

#define PROTODEMO_HEADER_ID "PBUFDEM"

struct protodemoheader_t
{
	char demo_file_stamp[8]; // PROTODEMO_HEADER_ID
	int32_t fileinfo_offset;
};

uint32_t read_var_int(std::istream &stream) {
  uint32_t b;
  int count = 0;
  uint32_t result = 0;

  do {
    XASSERT(count != 5, "Corrupt data.");
    XASSERT(!stream.eof(), "Premature end of stream.");

    b = stream.get();
    result |= (b & 0x7F) << (7 * count);
    ++count;
  } while (b & 0x80);

  return result;
}

Demo::Demo(const char *file) : buffer_len(0), scratch_len(0) {
  using namespace std;

  stream.open(file, ifstream::in | ifstream::binary);
  XASSERT(stream.is_open(), "Can't open file.");

  protodemoheader_t header;
  stream.read((char *) &header, sizeof(protodemoheader_t));
  XASSERT(!stream.fail(), "Failed to read header.");
  XASSERT(!strcmp(PROTODEMO_HEADER_ID, header.demo_file_stamp), "Wrong file stamp.");
}

Demo::~Demo() {
  if (stream.is_open()) {
    stream.close();
  }
}

char *Demo::expose_buffer() {
  return buffer;
}

bool Demo::eof() {
  stream.peek();
  return stream.eof();
}

size_t Demo::get_buffer_len() {
  return buffer_len;
}

EDemoCommands Demo::get_message_type(int *tick, bool *compressed) {
  uint32_t command = read_var_int(stream);

  *compressed = !!(command & DEM_IsCompressed);
  command = (command & ~DEM_IsCompressed);

  *tick = read_var_int(stream);

  XASSERT(!stream.eof(), "Premature end of stream.");

  return (EDemoCommands) command;
}

void Demo::read_message(bool compressed, size_t *size, size_t *uncompressed_size) {
  *size = read_var_int(stream);
  *uncompressed_size = 0;

  XASSERT(*size <= DEMO_BUFFER_SIZE, "Message longer than buffer size.");

  if (compressed) {
    stream.read(scratch, *size);
    scratch_len = *size;

    XASSERT(snappy::IsValidCompressedBuffer(scratch, scratch_len), "Invalid Snappy compression.");
    XASSERT(snappy::GetUncompressedLength(scratch, scratch_len, uncompressed_size), "Can't get length.");
    XASSERT(*uncompressed_size <= DEMO_BUFFER_SIZE, "Uncompressed message is longer than buffer.");
    XASSERT(snappy::RawUncompress(scratch, scratch_len, buffer), "Can't decompress.");
    buffer_len = *uncompressed_size;
  } else {
    stream.read(buffer, *size);
    buffer_len = *size;
    *uncompressed_size = *size;
  }
}

