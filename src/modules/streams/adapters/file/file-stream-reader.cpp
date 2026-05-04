#include "file-stream-reader.hpp"
#include "assert.hpp"
#include "log.hpp"
#include "stream-buffer.hpp"

namespace astralix {

FileStreamReader::FileStreamReader(const std::filesystem::path &path)
    : StreamReader(), m_path(path) {
  m_file.open(m_path, std::ios::binary);
  ASTRA_ENSURE(
      !m_file.is_open(), "Cannot open file ", m_path.string()
  );

  m_file.seekg(0, std::ios::end);
  const auto total_size = m_file.tellg();
  ASTRA_ENSURE(
      total_size < 0, "Cannot determine file size ", m_path.string()
  );

  m_total_size = static_cast<size_t>(total_size);

  m_file.seekg(0, std::ios::beg);

  m_buffer = create_scope<StreamBuffer>(m_total_size);
}

FileStreamReader::~FileStreamReader() {
}

void FileStreamReader::read() {
  ASTRA_ENSURE(
      !m_file.is_open(), "Cannot open file ", m_path.string()
  )

  if (m_total_size == 0u) {
    m_file.close();
    return;
  }

  m_file.read(m_buffer->data(), m_buffer->size());

  auto total_read = m_file.gcount();

  ASTRA_ENSURE(total_read != m_total_size,
               "Cannot read file ", m_path.string());

  if (m_file.eof()) {
    m_file.close();
  }
};

} // namespace astralix
//
