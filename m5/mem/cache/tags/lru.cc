/*
 * Copyright (c) 2003, 2004, 2005
 * The Regents of The University of Michigan
 * All Rights Reserved
 *
 * This code is part of the M5 simulator, developed by Nathan Binkert,
 * Erik Hallnor, Steve Raasch, and Steve Reinhardt, with contributions
 * from Ron Dreslinski, Dave Greene, Lisa Hsu, Kevin Lim, Ali Saidi,
 * and Andrew Schultz.
 *
 * Permission is granted to use, copy, create derivative works and
 * redistribute this software and such derivative works for any
 * purpose, so long as the copyright notice above, this grant of
 * permission, and the disclaimer below appear in all copies made; and
 * so long as the name of The University of Michigan is not used in
 * any advertising or publicity pertaining to the use or distribution
 * of this software without specific, written prior authorization.
 *
 * THIS SOFTWARE IS PROVIDED AS IS, WITHOUT REPRESENTATION FROM THE
 * UNIVERSITY OF MICHIGAN AS TO ITS FITNESS FOR ANY PURPOSE, AND
 * WITHOUT WARRANTY BY THE UNIVERSITY OF MICHIGAN OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE. THE REGENTS OF THE UNIVERSITY OF MICHIGAN SHALL NOT BE
 * LIABLE FOR ANY DAMAGES, INCLUDING DIRECT, SPECIAL, INDIRECT,
 * INCIDENTAL, OR CONSEQUENTIAL DAMAGES, WITH RESPECT TO ANY CLAIM
 * ARISING OUT OF OR IN CONNECTION WITH THE USE OF THE SOFTWARE, EVEN
 * IF IT HAS BEEN OR IS HEREAFTER ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGES.
 */

/**
 * @file
 * Definitions of LRU tag store.
 */

#include <string>
#include <fstream>
#include <iomanip>

#include "mem/cache/base_cache.hh"
#include "base/intmath.hh"
#include "mem/cache/tags/lru.hh"
#include "sim/root.hh"

using namespace std;

LRUBlk*
CacheSet::findBlk(int asid, Addr tag, int maxUseSets)
{
	int hitIndex = -1;
	return findBlk(asid, tag, &hitIndex, maxUseSets);
}

LRUBlk*
CacheSet::findBlk(int asid, Addr tag, int* hitIndex, int maxUseSets)
{
	for (int i = 0; i < assoc; ++i) {
		if (blks[i]->tag == tag && blks[i]->isValid()) {

			if(i >= maxUseSets){
				assert(lruTags->cache->isShared);
				assert(lruTags->cache->cpuCount == 1);
				return NULL;
			}

			*hitIndex = i;
			return blks[i];
		}
	}
	return 0;
}


void
CacheSet::moveToHead(LRUBlk *blk)
{
	// nothing to do if blk is already head
	if (blks[0] == blk)
		return;

	// write 'next' block into blks[i], moving up from MRU toward LRU
	// until we overwrite the block we moved to head.

	// start by setting up to write 'blk' into blks[0]
	int i = 0;
	LRUBlk *next = blk;

	do {
		assert(i < assoc);
		// swap blks[i] and next
		LRUBlk *tmp = blks[i];
		blks[i] = next;
		next = tmp;
		++i;
	} while (next != blk);
}

/* New address layout with banked caches (Magnus):
   Remember that all addresses are byte-addresses
 MSB                                                                 LSB
|----------------------------------------------------------------------|
| Tag         | Set index        | Bank | Block offset | Byte offset   |
|----------------------------------------------------------------------|
 */

// create and initialize a LRU/MRU cache structure
//block size is configured in bytes
LRU::LRU(int _numSets, int _blkSize, int _assoc, int _hit_latency, int _bank_count, bool _isShadow, int _divFactor, int _maxUseWays, int _shadowID) :
	numSets(_numSets), blkSize(_blkSize), assoc(_assoc), hitLatency(_hit_latency),numBanks(_bank_count),isShadow(_isShadow),divFactor(_divFactor)
	{

	// the provided addresses are byte addresses, so the provided block address can be used directly
	cache = NULL;
	cacheInterference = NULL;

	bankID = 0;

	// Check parameters
	if (blkSize < 4 || ((blkSize & (blkSize - 1)) != 0)) {
		fatal("Block size must be at least 4 and a power of 2");
	}
	if (numSets <= 0 || ((numSets & (numSets - 1)) != 0)) {
		fatal("# of sets must be non-zero and a power of 2");
	}
	if (assoc <= 0) {
		fatal("associativity must be greater than zero");
	}
	if (hitLatency <= 0) {
		fatal("access latency must be greater than zero");
	}
	if(_isShadow){
		assert(_shadowID != -1);
	}
	shadowID = _shadowID;

	LRUBlk  *blk;
	int i, j, blkIndex;

	blkMask = (blkSize) - 1;

	if(numBanks != -1){
		setShift = FloorLog2(blkSize) + FloorLog2(numBanks);
		bankShift = FloorLog2(blkSize);
	}
	else{
		setShift = FloorLog2(blkSize);
		bankShift = -1;
	}
	setMask = numSets - 1;
	tagShift = setShift + FloorLog2(numSets);
	warmedUp = false;
	/** @todo Make warmup percentage a parameter. */
	warmupBound = numSets * assoc;

	sets = new CacheSet[numSets];
	blks = new LRUBlk[numSets * assoc];
	// allocate data storage in one big chunk
	dataBlks = new uint8_t[numSets*assoc*blkSize];

	blkIndex = 0;	// index into blks array
	for (i = 0; i < numSets; ++i) {
		sets[i].assoc = assoc;
		sets[i].lruTags = this;

		sets[i].blks = new LRUBlk*[assoc];

		// link in the data blocks
		for (j = 0; j < assoc; ++j) {
			// locate next cache block
			blk = &blks[blkIndex];
			blk->data = &dataBlks[blkSize*blkIndex];
			++blkIndex;

			// invalidate new cache block
			blk->status = 0;

			//EGH Fix Me : do we need to initialize blk?

			// Setting the tag to j is just to prevent long chains in the hash
			// table; won't matter because the block is invalid
			blk->tag = j;
			blk->whenReady = 0;
			blk->asid = -1;
			blk->isTouched = false;
			blk->size = blkSize;
			sets[i].blks[j]=blk;
			blk->set = i;
		}
	}

	if(_isShadow){
		leaderSetHitDistribution.resize(assoc, 0);
	}

	if(_maxUseWays != -1){
		if(_isShadow) fatal("Maximum use ways does not make sense for shadow tags");
		if(_maxUseWays < 1 || _maxUseWays > assoc) fatal("Max use ways must be a number between 1 and the cache associativity");
		maxUseWays = _maxUseWays;
	}
	else{
		maxUseWays = assoc;
	}

	leaderSetAccesses = 0;
	doPartitioning = false;

	if(_isShadow && doPartitioning){
		fatal("Partitioning does not make sense for shadow tags");
	}
}

LRU::~LRU()
{
	delete [] dataBlks;
	delete [] blks;
	delete [] sets;
}

void
LRU::initializeCounters(int cpuCount){
	if(!isShadow && numBanks > 0){ // only L2, real tags
		perCPUperSetHitCounters.resize(cpuCount, vector<vector<int> >(numSets, vector<int>(assoc, 0)));

		if(cpuCount == 1 && divFactor < 1){
			fatal("A division factor must be given for single CPU static partitioning");
		}
	}
}

// probe cache for presence of given block.
bool
LRU::probe(int asid, Addr addr) const
{
	//  return(findBlock(Read, addr, asid) != 0);
	Addr tag = extractTag(addr);
	unsigned myset = extractSet(addr);

	LRUBlk *blk = sets[myset].findBlk(asid, tag, maxUseWays);

	return (blk != NULL);	// true if in cache
}

LRUBlk*
LRU::findBlock(Addr addr, int asid, int &lat)
{

	Addr tag = extractTag(addr);
	unsigned set = extractSet(addr);

	int hitIndex = -1;
	LRUBlk *blk = sets[set].findBlk(asid, tag, &hitIndex, maxUseWays);

	lat = hitLatency;
	if (blk != NULL) {
		// move this block to head of the MRU list
		sets[set].moveToHead(blk);

		if (blk->whenReady > curTick
				&& blk->whenReady - curTick > hitLatency) {
			lat = blk->whenReady - curTick;
		}
		blk->refCount += 1;
	}

	return blk;
}

LRUBlk*
LRU::findBlock(MemReqPtr &req, int &lat, bool isLeaderSet, int setsInConst)
{
	Addr addr = req->paddr;
	int asid = req->asid;

	Addr tag = extractTag(addr);
	unsigned set = extractSet(addr);

	int hitIndex = -1;
	LRUBlk *blk = sets[set].findBlk(asid, tag, &hitIndex, maxUseWays);

	if(isShadow){
		if(isLeaderSet) leaderSetAccesses++;

		if(blk != NULL){
			assert(hitIndex >= 0 && hitIndex < assoc);
			if(isLeaderSet) leaderSetHitDistribution[hitIndex] += setsInConst;
		}
		else{
			assert(hitIndex == -1);
		}
	}
	else{
		assert(!isLeaderSet);
	}

	lat = hitLatency;
	if (blk != NULL) {
		// move this block to head of the MRU list
		sets[set].moveToHead(blk);

		if (blk->whenReady > curTick
				&& blk->whenReady - curTick > hitLatency) {
			lat = blk->whenReady - curTick;
		}
		blk->refCount += 1;
	}

	return blk;
}

void
LRU::updateSetHitStats(MemReqPtr& req){

	assert(!isShadow); // only real tags
	if(curTick < cache->detailedSimulationStartTick) return;

	int hitIndex = -1;
	Addr tag = extractTag(req->paddr);
	unsigned set = extractSet(req->paddr);
	LRUBlk* tmpBlk = sets[set].findBlk(req->asid, tag, &hitIndex, maxUseWays);
	if(tmpBlk == NULL) return; // cache miss, no hit statistics :-)

	assert(req->adaptiveMHASenderID != -1);

	if(cache->isShared){
		assert(req->adaptiveMHASenderID == tmpBlk->origRequestingCpuID);
		assert(set >= 0 && set < perCPUperSetHitCounters[0].size());
		assert(hitIndex >= 0 && hitIndex < perCPUperSetHitCounters[0][0].size());
		perCPUperSetHitCounters[tmpBlk->origRequestingCpuID][set][hitIndex]++;

		cache->hitProfile[req->adaptiveMHASenderID].sample(hitIndex);
	}
	else{
		cache->hitProfile[0].sample(hitIndex);
	}
}

void
LRU::dumpHitStats(){
	stringstream name;
	name << cache->name() << "HitStats.txt";
	ofstream outfile(name.str().c_str());

	outfile << "Dumping hit statistics for " << cache->name() << " at tick " << curTick << "\n\n";

	for(int i=0;i<perCPUperSetHitCounters.size();i++){
		outfile << "CPU " << i << " hit statistics\n";
		for(int j=0;j<perCPUperSetHitCounters[0].size();j++){
			outfile << "Set " << setw(4) << right << j << setw(5) << left << ":";
			for(int k=0;k<perCPUperSetHitCounters[0][0].size();k++){
				outfile << setw(10) << left << perCPUperSetHitCounters[i][j][k];
			}
			outfile << "\n";
		}
		outfile << "\n";
	}

	outfile.flush();
	outfile.close();
}

LRUBlk*
LRU::findBlock(Addr addr, int asid) const
{
	Addr tag = extractTag(addr);
	unsigned set = extractSet(addr);
	LRUBlk *blk = sets[set].findBlk(asid, tag, maxUseWays);
	return blk;
}

std::vector<int>
LRU::getUsedBlocksPerCore(unsigned int set){
	vector<int> blkCnt(cache->cpuCount, 0);
	for(int i=0;i<assoc;i++){
		int tmpID = sets[set].blks[i]->origRequestingCpuID;
		if(tmpID >= 0){
			assert(tmpID >= 0 && tmpID < cache->cpuCount);
			blkCnt[tmpID]++;
		}
	}
	return blkCnt;
}

LRUBlk*
LRU::findLRUBlkForCPU(int cpuID, unsigned int set){
	// replace the LRU block belonging to this cache
	for(int i = assoc-1;i>=0;i--){
		LRUBlk* blk = sets[set].blks[i];
		if(blk->origRequestingCpuID == cpuID){
			return blk;
		}
	}
	return NULL;
}

LRUBlk*
LRU::findLRUOverQuotaBlk(vector<int> blocksInUse, unsigned int set){
	for(int i = assoc-1;i>=0;i--){
		LRUBlk* blk = sets[set].blks[i];
		assert(blk->origRequestingCpuID != -1);
		if(blocksInUse[blk->origRequestingCpuID] > currentPartition[blk->origRequestingCpuID]){
			return blk;
		}
	}
	return NULL;
}

LRUBlk*
LRU::findReplacement(MemReqPtr &req, MemReqList &writebacks,
		BlkList &compress_blocks)
{
	unsigned set = extractSet(req->paddr);

	// grab a replacement candidate
	LRUBlk *blk = NULL;
	if(doPartitioning){

		assert(req->adaptiveMHASenderID != -1);
		assert(currentPartition.size() == cache->cpuCount);
		assert(sets[set].blks[assoc-1] != NULL);

		int fromProc = req->adaptiveMHASenderID;
		int maxBlks = currentPartition[fromProc];
		vector<int> blocksInUse = getUsedBlocksPerCore(set);

		if(blocksInUse[fromProc] < maxBlks){
			if(!sets[set].blks[assoc-1]->isTouched){
				blk = sets[set].blks[assoc-1];
			}
			else{
				blk = findLRUOverQuotaBlk(blocksInUse, set);
			}
		}
		else{
			blk = findLRUBlkForCPU(fromProc, set);
		}
	}
	else{
		if(maxUseWays == assoc){
			blk = sets[set].blks[assoc-1];
		}
		else{
			assert(cache->isShared);
			assert(cache->cpuCount == 1);
			assert(maxUseWays > 0);
			blk = sets[set].blks[maxUseWays-1];
		}

	}
	assert(blk != NULL);

	//estimate interference
	if(blk->origRequestingCpuID != req->adaptiveMHASenderID && blk->origRequestingCpuID != -1 && !isShadow){
		assert(req->adaptiveMHASenderID != -1);
		cache->addCapacityInterference(blk->origRequestingCpuID, req->adaptiveMHASenderID);
	}

	sets[set].moveToHead(blk);
	if (blk->isValid()) {
		int thread_num = (blk->xc) ? blk->xc->thread_num : 0;
		replacements[thread_num]++;
		totalRefs += blk->refCount;
		++sampledRefs;
		blk->refCount = 0;
	} else if (!blk->isTouched) {
		tagsInUse++;
		blk->isTouched = true;
		if (!warmedUp && tagsInUse.value() >= warmupBound) {
			warmedUp = true;
			warmupCycle = curTick;
		}
	}
	return blk;
}

void
LRU::invalidateBlk(int asid, Addr addr)
{
	LRUBlk *blk = findBlock(addr, asid);
	if (blk) {
		blk->status = 0;
		blk->isTouched = false;
		tagsInUse--;
	}
}

void
LRU::doCopy(Addr source, Addr dest, int asid, MemReqList &writebacks)
{
	assert(source == blkAlign(source));
	assert(dest == blkAlign(dest));
	LRUBlk *source_blk = findBlock(source, asid);
	assert(source_blk);
	LRUBlk *dest_blk = findBlock(dest, asid);
	if (dest_blk == NULL) {
		// Need to do a replacement
		MemReqPtr req = new MemReq();
		req->paddr = dest;
		BlkList dummy_list;
		dest_blk = findReplacement(req, writebacks, dummy_list);
		if (dest_blk->isValid() && dest_blk->isModified()) {
			// Need to writeback data.
			req = buildWritebackReq(regenerateBlkAddr(dest_blk->tag,
					dest_blk->set),
					dest_blk->asid,
					dest_blk->xc,
					blkSize,
					(cache->doData())?dest_blk->data:0,
							dest_blk->size);
			writebacks.push_back(req);
		}
		dest_blk->tag = extractTag(dest);
		dest_blk->asid = asid;
		/**
		 * @todo Do we need to pass in the execution context, or can we
		 * assume its the same?
		 */
		assert(source_blk->xc);
		dest_blk->xc = source_blk->xc;
	}
	/**
	 * @todo Can't assume the status once we have coherence on copies.
	 */

	// Set this block as readable, writeable, and dirty.
	dest_blk->status = 7;
	if (cache->doData()) {
		memcpy(dest_blk->data, source_blk->data, blkSize);
	}
}

void
LRU::cleanupRefs()
{
	for (int i = 0; i < numSets*assoc; ++i) {
		if (blks[i].isValid()) {
			totalRefs += blks[i].refCount;
			++sampledRefs;
		}
	}
}


std::vector<int>
LRU::perCoreOccupancy(){
	vector<int> ret(cache->cpuCount, 0);
	int notTouched = 0;

	for(int i=0;i<numSets;i++){
		for(int j=0;j<assoc;j++){
			LRUBlk* blk = sets[i].blks[j];
			assert(blk->origRequestingCpuID < cache->cpuCount);
			if(blk->origRequestingCpuID != -1
					&& blk->isTouched){
				ret[blk->origRequestingCpuID]++;
			}
			else{
				notTouched++;
			}
		}
	}

	ret.push_back(notTouched);
	ret.push_back(numSets * assoc);

	int sum = 0;
	for(int i=0;i<cache->cpuCount+1;i++) sum += ret[i];
	assert(sum == numSets * assoc);

	return ret;
}

void
LRU::resetHitCounters(){
	DPRINTF(CachePartitioning, "Resetting shadow tag hit counters for ID %d\n", shadowID);
	DPRINTF(MissBWPolicy, "Resetting shadow tag hit counters for ID %d\n", shadowID);
	leaderSetAccesses = 0;
	for(int j=0;j<assoc;j++){
		leaderSetHitDistribution[j] = 0;
	}
}

std::vector<int>
LRU::getHitDistribution(){
	assert(isShadow);

	vector<int> hits = leaderSetHitDistribution;

	// transform to cumulative representation
	for(int i=1;i<hits.size();i++){
		hits[i] = hits[i] + hits[i-1];
	}

	return hits;
}

std::vector<double>
LRU::getMissRates(){

	assert(isShadow);

	vector<int> hits = getHitDistribution();

	// compute miss rates
	vector<double> missrates(assoc, 0);
	assert(missrates.size() == hits.size());
	if(leaderSetAccesses > 0){
		for(int i=0;i<missrates.size();i++){
			missrates[i] = (double) (((double) leaderSetAccesses - (double) hits[i]) / (double) leaderSetAccesses);
		}
	}

	return missrates;
}

double
LRU::getTouchedRatio(){

	int warmcnt = 0;
	int totalcnt = 0;

	for(int i=0;i<numSets;i++){
		for(int j=0;j<assoc;j++){
			if(sets[i].blks[j]->isTouched) warmcnt++;
			totalcnt++;
		}
	}
	assert(totalcnt == numSets*assoc);
	return (double) ((double) warmcnt / (double) totalcnt);
}

void
LRU::enablePartitioning(){
	if(isShadow) fatal("Cache partitioning does not make sense for shadow tags");
	doPartitioning = true;
}
void
LRU::setCachePartition(std::vector<int> setQuotas){

	int setcnt = 0;
	for(int i=0;i<setQuotas.size();i++) setcnt += setQuotas[i];

	DPRINTF(CachePartitioning, "Enforcing set quotas:");
	for(int i=0;i<setQuotas.size();i++){
		DPRINTFR(CachePartitioning, " %d:%d", i, setQuotas[i]);
	}
	DPRINTFR(CachePartitioning, "\n");

	assert(setcnt == assoc);

	assert(setQuotas.size() == cache->cpuCount);
	currentPartition = setQuotas;
}

string
LRU::generateIniName(string cachename, int set, int pos){
	stringstream tmp;
	tmp << cachename << ".blk_" << set << "_" << pos;
	return tmp.str();
}

void
LRU::serialize(std::ostream &os, std::string name){

	int dumpSets = assoc;
	if(cache != NULL){
		if(cache->isShared) dumpSets = assoc / divFactor;
	}
	if(cache == NULL) dumpSets = assoc / divFactor;

	stringstream filenamestream;
	filenamestream << name << "-content.bin";
	string filename(filenamestream.str());
	SERIALIZE_SCALAR(filename);

	ofstream contentfile(filename.c_str(), ios::binary | ios::trunc);

	int expectedBlocks = numSets*dumpSets;
	Serializable::writeEntry(&expectedBlocks, sizeof(int), contentfile);

	for(int i=0;i<numSets;i++){
		for(int j=0;j<dumpSets;j++){
			sets[i].blks[j]->serialize(contentfile);
		}
	}

	contentfile.flush();
	contentfile.close();
}

Addr
LRU::regenerateChkBlkAddr(Addr set, Addr tag, int origTagShift){

	if(bankShift == -1){
		return ((tag << origTagShift) | ((Addr)set << setShift));
	}
	return ((tag << origTagShift) | ((Addr)set << setShift) | ((Addr) bankID << bankShift ));
}

void
LRU::unserialize(Checkpoint *cp, const std::string &section, string _filename){

	if(cache != NULL){
		if(cache->getLLCCheckpointLoadDisabled()){
			DPRINTF(Serialize, "Checkpoint load disabled for %s, returning\n", cache->name());
			return;
		}
	}
	else{
		assert(cacheInterference != NULL);
		if(cacheInterference->getLLCCheckpointLoadDisabled()){
			DPRINTF(Serialize, "Checkpoint load disabled for %s, returning\n", cacheInterference->name());
			return;
		}
	}

	string filename = _filename;
	if(filename == "") UNSERIALIZE_SCALAR(filename);

	ifstream contentfile(filename.c_str(), ios::binary);
	if(!contentfile.is_open()){
		fatal("could not open file %s", filename.c_str());
	}

	int* fileBlocks = (int*) Serializable::readEntry(sizeof(int), contentfile);
	int readAssoc = assoc;
	if(cache != NULL){
		if(cache->isShared && cache->cpuCount == 1) readAssoc = assoc / divFactor;
	}
	else{
		assert(cacheInterference != NULL);
		if(cacheInterference->cpuCount == 1) readAssoc = assoc / divFactor;
		else readAssoc = assoc / cacheInterference->cpuCount;
	}

	DPRINTF(Serialize, "Unserializing cache from file %s, file blocks %d, read associativity %d, cache associativity %d, total sets %d\n",
			filename.c_str(),
			*fileBlocks,
			readAssoc,
			assoc,
			numSets);

	if(*fileBlocks < numSets*readAssoc){
		fatal("File %s contains too few blocks for cache, got blocks %d, in cache %d", filename, *fileBlocks, numSets*readAssoc);
	}

	int checkpointSets = *fileBlocks / readAssoc;
	int origTagShift = setShift + FloorLog2(checkpointSets);

	for(int i=0;i<numSets;i++){
		DPRINTF(Serialize, "Unserializing cache set %d of %d\n",i,numSets);
		for(int j=0;j<readAssoc;j++){
			DPRINTF(Serialize, "Unserializing cache block %d of set %d\n", j, i);
			assert(contentfile.good());
			sets[i].blks[j]->unserialize(contentfile);

			if(checkpointSets > numSets){
				Addr origAddr = regenerateChkBlkAddr(sets[i].blks[j]->set,
													 sets[i].blks[j]->tag,
													 origTagShift);

				assert(extractSet(origAddr) == i);
				sets[i].blks[j]->set = i;
				sets[i].blks[j]->tag = extractTag(origAddr);
				DPRINTF(Serialize, "Generating new tag %d for address %d\n",
						sets[i].blks[j]->tag,
						origAddr);
			}

			if(sets[i].blks[j]->isValid()){
				sets[i].blks[j]->isTouched = true;
				tagsInUse++;
			}

			if((cache != NULL ? cache->cpuCount : cacheInterference->cpuCount) > 1){
				int blockAddrCPUID = -1;
				if(isShadow){
					if(sets[i].blks[j]->isValid()){
						assert(shadowID >= 0);
						blockAddrCPUID = shadowID;
					}
					else{
						assert(sets[i].blks[j]->origRequestingCpuID == -1);
					}

				}
				else if(cache->isShared){
					blockAddrCPUID = sets[i].blks[j]->origRequestingCpuID;
					if(blockAddrCPUID == -1) assert(!sets[i].blks[j]->isValid());
				}
				else{
					assert(cache->cacheCpuID != -1);
					blockAddrCPUID = cache->cacheCpuID;
				}

				if(blockAddrCPUID != -1){
					Addr paddr = regenerateBlkAddr(sets[i].blks[j]->tag, sets[i].blks[j]->set);
					Addr relocatedAddr = 0;
					if(cache != NULL){
						relocatedAddr = cache->relocateAddrForCPU(blockAddrCPUID, paddr, cache->cpuCount);
					}
					else{
						assert(cacheInterference != NULL);
						relocatedAddr = cacheInterference->relocateAddrForCPU(blockAddrCPUID, paddr, cacheInterference->cpuCount);
					}

					sets[i].blks[j]->tag = extractTag(relocatedAddr);
					sets[i].blks[j]->set = extractSet(relocatedAddr);

					DPRINTF(Serialize, "Relocating address %d to address %d new set %d and new tag %d\n",
							paddr,
							relocatedAddr,
							sets[i].blks[j]->set,
							sets[i].blks[j]->tag);
				}
			}
		}
	}

	if(maxUseWays < readAssoc){
		for(int i=0;i<numSets;i++){
			for(int j=maxUseWays;j<readAssoc;j++){
				sets[i].blks[j]->status = 0; //Invalidate block
				sets[i].blks[j]->isTouched = false;
				assert(!sets[i].blks[j]->isValid());
			}
		}
	}


	contentfile.close();
}
