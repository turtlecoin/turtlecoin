// Copyright (c) 2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include "MainChainStorageRocksdb.h"

#include <Common/FileSystemShim.h>

#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

#include "Common/CryptoNoteTools.h"

#include "rocksdb/db.h"
#include "rocksdb/options.h"
#include "rocksdb/cache.h"
#include "rocksdb/table.h"

using namespace rapidjson;
using namespace CryptoNote;


namespace CryptoNote
{
    MainChainStorageRocksdb::MainChainStorageRocksdb(
      const std::string &blocksFilename, 
      const std::string &indexesFilename, 
      const DataBaseConfig& config)
    {
        /* setup db options */
        rocksdb::DBOptions dbOpts;
        dbOpts.create_if_missing = true;
        dbOpts.IncreaseParallelism(config.getBackgroundThreadsCount());
        dbOpts.info_log_level = rocksdb::InfoLogLevel::WARN_LEVEL;
        dbOpts.max_open_files = -1;
        dbOpts.keep_log_file_num = 3;
        dbOpts.recycle_log_file_num = 2;
        
        /* setup column family options */
        rocksdb::ColumnFamilyOptions cfOpts;
        cfOpts.target_file_size_base = 32 * 1024 * 1024;
        cfOpts.max_bytes_for_level_base = config.getWriteBufferSize();
        cfOpts.target_file_size_multiplier = 2;
        cfOpts.level0_file_num_compaction_trigger = 20;
        cfOpts.level0_slowdown_writes_trigger = 30;
        cfOpts.level0_stop_writes_trigger = 40;
        cfOpts.write_buffer_size = 256 * 1024 * 1024;
        cfOpts.min_write_buffer_number_to_merge = 2;
        cfOpts.max_write_buffer_number = 6;
        cfOpts.num_levels = 10;
        cfOpts.compaction_style = rocksdb::kCompactionStyleLevel;
        
        const auto compressionLevel = config.getCompressionEnabled() ? rocksdb::kLZ4Compression : rocksdb::kNoCompression;
        std::fill_n(std::back_inserter(cfOpts.compression_per_level), cfOpts.num_levels, compressionLevel);
        cfOpts.bottommost_compression = config.getCompressionEnabled() ? rocksdb::kLZ4HCCompression : rocksdb::kNoCompression;
        
        rocksdb::BlockBasedTableOptions tblOpts;
        tblOpts.block_cache = rocksdb::NewLRUCache(32 * 1024 * 1024);
        std::shared_ptr<rocksdb::TableFactory> tf(NewBlockBasedTableFactory(tblOpts));
        cfOpts.table_factory = tf;
        
        rocksdb::Options options = rocksdb::Options(dbOpts, cfOpts);
        
        /* open the db, use temp pointer */
        rocksdb::DB* db;
        rocksdb::Status s = rocksdb::DB::Open(options, blocksFilename, &db);
        if (!s.ok()) 
        {
            throw std::runtime_error("Failed to load main chain storage from " + blocksFilename + ": " + s.ToString());
        }
        /* grab ownership of db, so it managed by the unique_ptr (m_db) */
        m_db.reset(db);
        
        /* initialize block count cache */
        initializeBlockCount();
    }

    MainChainStorageRocksdb::~MainChainStorageRocksdb()
    {
        m_db->Flush(rocksdb::FlushOptions());
        m_db->SyncWAL();
    }

    void MainChainStorageRocksdb::pushBlock(const RawBlock &rawBlock)
    {
        /* stringify RawBlock for storage **/
        StringBuffer rblock;
        Writer<StringBuffer> writer(rblock);
        rawBlock.toJSON(writer);
        rocksdb::WriteBatch batch;
        
        /* insert new block */
        batch.Put(std::to_string(m_blockcount), rblock.GetString());
        /* update the block count */
        batch.Put("count", std::to_string(m_blockcount+1));
        rocksdb::Status s = m_db->Write(rocksdb::WriteOptions(), &batch);
        
        if( !s.ok() ) 
        {
            throw std::runtime_error("Failed to insert new block: " + s.ToString());
        }
        
        /* increment cached block counter */
        m_blockcount++;
    }
    
    void MainChainStorageRocksdb::popBlock()
    {
        /* don't do anything if we don't have any entries */
        if( m_blockcount == 0)
        {
            return;
        }
        
        /* set write options to write in sync mode */
        rocksdb::WriteOptions write_options;
        write_options.sync = true;
        
        rocksdb::WriteBatch batch;
        /* delete the last block record */
        batch.Delete(std::to_string(m_blockcount));
        /* update the block count */
        batch.Put("count", std::to_string(m_blockcount-1));
        rocksdb::Status s = m_db->Write(write_options, &batch);
        
        if( !s.ok() )
        {
            throw std::runtime_error("Failed to pop the last block off the database: " + s.ToString());
        }
        
        /* decrement cached block counter */
        m_blockcount--;
    }
    
    void MainChainStorageRocksdb::rewindTo(const uint32_t index) const
    {
        /* return early if we don't have enough record as requested */
        if( index >= m_blockcount )
        {
            return;
        }
      
        /* Range of blocks to be deleted. 
           start: target rewind index/height, 
           end: max block index we have in db */
        rocksdb::Slice start = std::to_string(index);
        rocksdb::Slice end = std::to_string(m_blockcount-1); 
      
        /* set write options to write in sync mode */
        rocksdb::WriteOptions write_opts;
        write_opts.sync = true;
        auto cf = m_db->DefaultColumnFamily();

        /* perform range deletion */
        rocksdb::Status s = m_db->DeleteRange(write_opts, cf, start, end);
      
        if( !s.ok() )
        {
            throw std::runtime_error("Rewind operation failed: " + s.ToString());
        }
      
        /* update block count, new count == rewind index/height */
        s = m_db->Put(write_opts, "count", std::to_string(index));
      
        if (!s.ok())
        {
            throw std::runtime_error("Rewind operation failed: " + s.ToString());
        }
      
        m_db->Flush(rocksdb::FlushOptions());
        m_db->SyncWAL();
    }

    RawBlock MainChainStorageRocksdb::getBlockByIndex(uint32_t index) const
    {
        /* get the serialized RawBlock from db */
        rocksdb::PinnableSlice rawBlockString;
        rocksdb::Status s = m_db->Get(
            rocksdb::ReadOptions(),
            m_db->DefaultColumnFamily(),
            std::to_string(index),
            &rawBlockString
        );
        
        /* parse it back to RawBlock */
        Document doc;
        if (doc.Parse<0>(rawBlockString.ToString()).HasParseError() ) {
            throw std::runtime_error("Failed to get block by index: unable to parse block data");
        }
        
        RawBlock rawBlock;
        rawBlock.fromJSON(doc);
        return rawBlock;
    }
    
    void MainChainStorageRocksdb::initializeBlockCount() 
    {
        m_blockcount = 0;
        
        rocksdb::PinnableSlice count;
        rocksdb::Status s = m_db->Get(rocksdb::ReadOptions(), m_db->DefaultColumnFamily(), "count", &count);

        if(s.ok())
        {
            /* got the block count, use it as initial value for m_blockcount */
            m_blockcount = std::stoi(count.ToString());
        }
        else if ( s.IsNotFound() )
        {
            /* "count" key is not found (newly created db), so add the key and set value to zero */
            rocksdb::WriteOptions write_options;
            write_options.sync = true;
            s = m_db->Put(write_options, "count", "0");
            if (!s.ok()) 
            {
                throw std::runtime_error("Failed to initialize block count: " + s.ToString());
            }
        }
        else
        {
            throw std::runtime_error("Failed to to initialize block count: " + s.ToString());
        }
    }

    uint32_t MainChainStorageRocksdb::getBlockCount() const
    {
        return m_blockcount;        
    }

    void MainChainStorageRocksdb::clear()
    {
        /* do nothing if we don't have any block */
        if( m_blockcount == 0 )
        {
            return;
        }
        
        rocksdb::Slice start = "0";
        rocksdb::Slice end = std::to_string(m_blockcount);
        auto cf = m_db->DefaultColumnFamily();
        rocksdb::WriteOptions write_options;
        write_options.sync = true;
        
        rocksdb::Status s = m_db->DeleteRange(write_options, cf, start, end);
        if( !s.ok() ) 
        {
            throw std::runtime_error("Failed to clear blocks: " + s.ToString());
        }
        
        s = m_db->Put(write_options, "count", "0");
        if (!s.ok())
        {
            throw std::runtime_error("Failed to update block count: " + s.ToString());
        }
        /* reset m_blockcount value */
        m_blockcount = 0;
    }

    std::unique_ptr<IMainChainStorage> createSwappedMainChainStorageRocksdb(
      const std::string &dataDir,
      const Currency &currency,
      const DataBaseConfig& config
      )
    {
        fs::path blocksFilename = fs::path(dataDir) / currency.blocksFileName();
        fs::path indexesFilename = fs::path(dataDir) / currency.blockIndexesFileName();

        auto storage = std::make_unique<MainChainStorageRocksdb>(
            blocksFilename.string() + ".rocksdb",
            indexesFilename.string(),
            config
        );

        if (storage->getBlockCount() == 0)
        {
            RawBlock genesisBlock;
            genesisBlock.block = toBinaryArray(currency.genesisBlock());
            storage->pushBlock(genesisBlock);
        }

        return storage;
    }
}