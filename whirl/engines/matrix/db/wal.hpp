#pragma once

#include <whirl/fs/fs.hpp>
#include <whirl/fs/io.hpp>

#include <whirl/io/framed.hpp>

#include <whirl/cereal/serialize.hpp>

#include <whirl/db/kv.hpp>

namespace whirl::matrix::db {

// Write ahead log (WAL) writer / reader

//////////////////////////////////////////////////////////////////////

class WALWriter {
 public:
  WALWriter(node::fs::IFileSystem* fs, node::fs::FsPath file_path)
      : file_writer_(fs, file_path), framed_writer_(&file_writer_) {
  }

  void Put(node::db::Key key, node::db::Value value) {
    Append({key, value, node::db::MutationType::Put});
  }

  void Delete(node::db::Key key) {
    Append({key, std::nullopt, node::db::MutationType::Delete});
  }

 private:
  // Atomic
  void Append(node::db::Mutation mut) {
    framed_writer_.WriteFrame(Serialize(mut));
  }

 private:
  node::fs::FileWriter file_writer_;
  FramedWriter framed_writer_;
};

//////////////////////////////////////////////////////////////////////

class WALReader {
 public:
  WALReader(node::fs::IFileSystem *fs, node::fs::FsPath log_file_path)
      : file_reader_(fs, log_file_path),
        framed_reader_(&file_reader_) {
  }

  std::optional<node::db::Mutation> Next() {
    auto frame = framed_reader_.ReadNextFrame();
    if (!frame.has_value()) {
      return std::nullopt;
    }
    return Deserialize<node::db::Mutation>(*frame);
  }

 private:
  node::fs::FileReader file_reader_;
  FramedReader framed_reader_;
};

}  // namespace whirl::matrix::db
