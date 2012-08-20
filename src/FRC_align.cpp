/*
    FRC: computes the FRC curve starting from alignments
    Copyright (C) 2011  F. Vezzi(vezi84@gmail.com)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
 
#include <stdio.h> 
#include <time.h>
#include <string>
#include <vector>
#include <map>
#include "radix.h"
#include "samtools/sam.h"
#include "options/Options.h"


#include "data_structures/Features.h"
#include "data_structures/Contig.h"
#include "data_structures/FRC.h"

#define MIN(x,y) \
  ((x) < (y)) ? (x) : (y)

#define EXIT_IF_NULL(P) \
  if (P == NULL) \
    return 1;

/**
 * Check if read is properly mapped
 * @return true if read mapped, false otherwise
 */
static bool is_mapped(const bam1_core_t *core)
{

  if (core->flag&BAM_FUNMAP) {
    return false;
  }

  return true;
}


/**
 * Open a .sam/.bam file.
 * @returns NULL is open failed.
 */
samfile_t * open_alignment_file(std::string path)
{
  samfile_t * fp = NULL;
  std::string flag = "r";
  if (path.substr(path.size()-3).compare("bam") == 0) {
    //BAM file!
    flag += "b";
  }
  if ((fp = samopen(path.c_str(), flag.c_str() , 0)) == 0) {
    fprintf(stderr, "qaCompute: Failed to open file %s\n", path.c_str());
  }
  return fp;
}



struct LibraryStatistics{
	float C_A;
	float S_A;
	float C_C;
	float C_M;
	float C_S;
	float C_W;
	float insertMean;
	float insertStd;
};

LibraryStatistics computeLibraryStats(samfile_t *fp, unsigned int minInsert, unsigned int maxInsert, uint64_t genomeLength);




void printLOW_COV_PE_FRC(string outputFile, unsigned int HIGH_SINGLE_PE_features, unsigned long estimatedGenomeSize, FRC frc);
void printHIGH_COV_PE_FRC(string outputFile, unsigned int HIGH_SINGLE_PE_features, unsigned long estimatedGenomeSize, FRC frc);
void printLOW_NORM_COV_PE_FRC(string outputFile, unsigned int HIGH_SINGLE_PE_features, unsigned long estimatedGenomeSize, FRC frc);
void printHIGH_NORM_COV_PE_FRC(string outputFile, unsigned int HIGH_SINGLE_PE_features, unsigned long estimatedGenomeSize, FRC frc);
void printHIGH_SINGLE_PE_FRC(string outputFile, unsigned int HIGH_SINGLE_PE_features, unsigned long estimatedGenomeSize, FRC frc);
void printHIGH_OUTIE_PE_FRC(string outputFile, unsigned int HIGH_SINGLE_PE_features, unsigned long estimatedGenomeSize, FRC frc);
void printHIGH_SPAN_PE_FRC(string outputFile, unsigned int HIGH_SINGLE_PE_features, unsigned long estimatedGenomeSize, FRC frc);
void printCOMPR_PE_FRC(string outputFile, unsigned int HIGH_SINGLE_PE_features, unsigned long estimatedGenomeSize, FRC frc);
void printSTRECH_PE_FRC(string outputFile, unsigned int HIGH_SINGLE_PE_features, unsigned long estimatedGenomeSize, FRC frc);

void printHIGH_SINGLE_MP_FRC(string outputFile, unsigned int HIGH_SINGLE_PE_features, unsigned long estimatedGenomeSize, FRC frc);
void printHIGH_OUTIE_MP_FRC(string outputFile, unsigned int HIGH_SINGLE_PE_features, unsigned long estimatedGenomeSize, FRC frc);
void printHIGH_SPAN_MP_FRC(string outputFile, unsigned int HIGH_SINGLE_PE_features, unsigned long estimatedGenomeSize, FRC frc);
void printCOMPR_MP_FRC(string outputFile, unsigned int HIGH_SINGLE_PE_features, unsigned long estimatedGenomeSize, FRC frc);
void printSTRECH_MP_FRC(string outputFile, unsigned int HIGH_SINGLE_PE_features, unsigned long estimatedGenomeSize, FRC frc);



int main(int argc, char *argv[]) {
	//MAIN VARIABLE
	string assemblyFile = "";

	string PEalignmentFile = "";
	vector<string> peFiles;
	int32_t peMinInsert = 100;
	int32_t peMaxInsert = 1000000;

	string MPalignmentFile = "";
	vector<string> mpFiles;
	int32_t mpMinInsert = 100;
	int32_t mpMaxInsert = 1000000;
	unsigned int WINDOW = 1000;
	uint64_t estimatedGenomeSize;

	float CEstats_PE_min = -3.2;
	float CEstats_PE_max = +4.5;

	float CEstats_MP_min = -6.5;
	float CEstats_MP_max = +6.1;

	string outputFile = "FRC.txt";
	string featureFile = "Features.txt";

	// PROCESS PARAMETERS
	stringstream ss;
	ss << package_description() << endl << endl << "Allowed options";
	po::options_description desc(ss.str().c_str());
	desc.add_options() ("help", "produce help message")
			("pe-sam", po::value<string>(), "paired end alignment file (in sam or bam format). Expected orientation -> <-")
			("pe-min-insert",  po::value<int>(), "paired reads minimum allowed insert size. Used in order to filter outliers. Insert size goeas from beginning of first read to end of second read")
			("pe-max-insert",  po::value<int>(), "paired reads maximum allowed insert size. Used in order to filter outliers.")
			("mp-sam", po::value<string>(), "mate pairs alignment file. (in sam or bam format). Expected orientation -> <- (reverse and complement originals)")
			("mp-min-insert",  po::value<int>(), "mate pairs minimum allowed insert size. Used in order to filter outliers. Insert size goeas from beginning of first read to end of second read")
			("mp-max-insert",  po::value<int>(), "mate pairs maximum allowed insert size. Used in order to filter outliers.")
			("window",  po::value<unsigned int>(), "window size for features computation")
			("genome-size", po::value<unsigned long int>(), "estimated genome size (if not supplied genome size is believed to be assembly length")
			("output",  po::value<string>(), "Header output file names (default FRC.txt and Features.txt)")
			("assembly", po::value<string>(), "assembly file name in fasta format [FOR FUTURE USE]")
			("CEstats-PE-min", po::value<float>(), "minimum allowed CE_stats in PE library")
			("CEstats-PE-max", po::value<float>(), "maximum allowed CE_stats in PE library")
			("CEstats-MP-min", po::value<float>(), "minimum allowed CE_stats in MP library")
			("CEstats-MP-max", po::value<float>(), "maximum allowed CE_stats in MP library")
			("pe", po::value< vector < string > >(), "paired reads, one pair after the other [FOR FUTURE USE]")
			("mp", po::value< vector < string > >(), "mate pairs, one pair after the other [FOR FUTURE USE]")
			;

	po::variables_map vm;
	try {
		po::store(po::parse_command_line(argc, argv, desc), vm);
		po::notify(vm);
	} catch (boost::program_options::error & error) {
		ERROR_CHANNEL <<  error.what() << endl;
		ERROR_CHANNEL << "Try \"--help\" for help" << endl;
		exit(2);
	}
	if (vm.count("help")) {
		DEFAULT_CHANNEL << desc << endl;
		exit(0);
	}

//PARSE CE STATS (if present)
	if (vm.count("CEstats-PE-min")) {
		CEstats_PE_min = vm["CEstats-PE-min"].as<float>();
	}
	if (vm.count("CEstats-PE-min")) {
		CEstats_PE_max = vm["CEstats-PE-max"].as<float>();
	}
	if (vm.count("CEstats-PE-min")) {
		CEstats_MP_min = vm["CEstats-MP-min"].as<float>();
	}
	if (vm.count("CEstats-PE-min")) {
		CEstats_MP_max = vm["CEstats-MP-max"].as<float>();
	}

	cout << "CEstats-PE-min " << CEstats_PE_min << "\n";
	cout << "CEstats-PE-max " << CEstats_PE_max << "\n";
	cout << "CEstats-MP-min " << CEstats_MP_min << "\n";
	cout << "CEstats-MP-max " << CEstats_MP_max << "\n";

// PARSE PE
	if (!vm.count("pe-sam") && !vm.count("mp-sam")) {
		DEFAULT_CHANNEL << "At least one library must be present. Please specify at least one between pe-sam and mp-sam" << endl;
		exit(0);
	}

	if (vm.count("pe-sam")) {
		PEalignmentFile = vm["pe-sam"].as<string>();
	}
	if (vm.count("pe-min-insert")) {
		peMinInsert = vm["pe-min-insert"].as<int>();
		if(peMinInsert <= 0) {
			DEFAULT_CHANNEL << "pe minimum insert should be at least 1\n";
			DEFAULT_CHANNEL << desc << endl;
			exit(2);
		}
	}
	if (vm.count("pe-max-insert")) {
		peMaxInsert = vm["pe-max-insert"].as<int>();
	}
// NOW PARSE MP
	if (vm.count("mp-sam")) {
		MPalignmentFile = vm["mp-sam"].as<string>();
	}
	if (vm.count("mp-min-insert")) {
		mpMinInsert = vm["mp-min-insert"].as<int>();
		if(mpMinInsert <= 0) {
			DEFAULT_CHANNEL << "mp minimum insert should be at least 1\n";
			DEFAULT_CHANNEL << desc << endl;
			exit(2);
		}
	}
	if (vm.count("mp-max-insert")) {
		mpMaxInsert = vm["mp-max-insert"].as<int>();
	}


	if(vm.count("pe-sam")){
		cout << "pe-sam file name is " << PEalignmentFile << endl;
		cout << "pe min " << peMinInsert << "\n";
		cout << "pe max " << peMaxInsert << "\n";
	}
	if(vm.count("mp-sam")){
		cout << "mp-sam file name is " << MPalignmentFile << endl;
		cout << "mp min " << mpMinInsert << "\n";
		cout << "mp max " << mpMaxInsert << "\n";
	}



	if (vm.count("assembly")) {
		assemblyFile = vm["assembly"].as<string>();
	}
	if (vm.count("pe")) {
		peFiles = vm["pe"].as<vector<string> >();
	}


	if (vm.count("window")) {
		WINDOW = vm["window"].as<unsigned int>();
	}

	string header = "";
	if (vm.count("output")) {
		header = vm["output"].as<string>();
		outputFile = header + "_FRC.txt";
		featureFile = header + "_Features.txt";
	}

	if (vm.count("genome-size")) {
		estimatedGenomeSize = vm["genome-size"].as<unsigned long int>();
	} else {
		estimatedGenomeSize = 0;
	}

//TODO: PARSING ENDED, CREATE A FUNCTION FOR IT
	uint64_t genomeLength = 0;
	uint32_t contigsNumber = 0;
	samfile_t *fp;
	if(vm.count("pe-sam")) { // paired read library is preset, use it to compute basic contig statistics
		fp = open_alignment_file(PEalignmentFile);
	} else {  // Otherwise use MP library that must be provided
		fp = open_alignment_file(MPalignmentFile);
	}

	EXIT_IF_NULL(fp);
	bam_header_t* head = fp->header; // sam header
	map<string,unsigned int> contig2position;
	map<unsigned int,string> position2contig;
	for(int i=0; i< head->n_targets ; i++) {
		genomeLength += head->target_len[i];
		contig2position[head->target_name[i]]=contigsNumber; // keep track of contig name and position in order to avoid problems when processing two libraries
		position2contig[contigsNumber] = head->target_name[i]; //
		contigsNumber++;
	}
	if (estimatedGenomeSize == 0) {
		estimatedGenomeSize = genomeLength; // if user has not provided genome size, approaximated it with assembly size
	}
	cout << "total number of contigs " << contigsNumber << endl;
	cout << "assembly length " << genomeLength << "\n";
	cout << "estimated length " << estimatedGenomeSize << "\n";

    LibraryStatistics libraryPE;
    LibraryStatistics libraryMP;
    uint32_t peMinInsert_recomputed; // recompute min and max insert on the basis of the new insert size
    uint32_t peMaxInsert_recomputed; // the original min and max threshold are used only as a first rough approximation
    uint32_t mpMinInsert_recomputed; // recompute min and max insert on the basis of the new insert size
    uint32_t mpMaxInsert_recomputed; // the original min and max threshold are used only as a first rough approximation

    unsigned int timesStdDev = 3;
	if(vm.count("pe-sam")) { // in this case file is already OPEN
		cout << "COMPUTING PE STATISTIC\n";
		libraryPE = computeLibraryStats(fp, peMinInsert, peMaxInsert, estimatedGenomeSize);
		samclose(fp); // close the file
	    if(libraryPE.insertMean - timesStdDev*libraryPE.insertStd > 0) {
	    	peMinInsert_recomputed = libraryPE.insertMean - timesStdDev*libraryPE.insertStd;
	    } else {
	    	peMinInsert_recomputed = 0;
	    }
	    peMaxInsert_recomputed = libraryPE.insertMean + timesStdDev*libraryPE.insertStd;
	    cout << "\tNEW minimum allowed insert " << peMinInsert_recomputed << "\n";
	    cout << "\tNEW maximum allowed insert " << peMaxInsert_recomputed << "\n";
	}

	fp = open_alignment_file(PEalignmentFile);


	timesStdDev = 3;
	if(vm.count("mp-sam")) {
		cout << "COMPUTING MP STATISTIC\n";
		if(!vm.count("pe-sam")) { // in this case file is already OPEN
			libraryMP = computeLibraryStats(fp, mpMinInsert, mpMaxInsert, estimatedGenomeSize);
			samclose(fp); // close the file
		} else { // in this case I need to open file first
			fp = open_alignment_file(MPalignmentFile);
			libraryMP = computeLibraryStats(fp, mpMinInsert, mpMaxInsert, estimatedGenomeSize);
			samclose(fp); // close the file
		}
		if(libraryMP.insertMean - timesStdDev*libraryMP.insertStd > 0) {
			mpMinInsert_recomputed = libraryMP.insertMean - timesStdDev*libraryMP.insertStd;
		} else {
			mpMinInsert_recomputed = 0;
		}
		mpMaxInsert_recomputed = libraryMP.insertMean + timesStdDev*libraryMP.insertStd;

		cout << "\tNEW minimum allowed insert " << mpMinInsert_recomputed << "\n";
		cout << "\tNEW maximum allowed insert " << mpMaxInsert_recomputed << "\n";
	}





    //parse BAM file again to compute FRC curve
    FRC frc = FRC(contigsNumber); // FRC object, will memorize all information on features and contigs
    if(vm.count("pe-sam")) { // paired read library is preset, use it to compute basic contig statistics
    	fp = open_alignment_file(PEalignmentFile);
    } else {  // Otherwise use MP library that must be provided
    	fp = open_alignment_file(MPalignmentFile);
    }
    EXIT_IF_NULL(fp);
    head = fp->header; // sam header
    uint32_t featuresTotal = 0;
    for(int i=0; i< head->n_targets ; i++) { // Initialize FRC structure
    	frc.setContigLength(i, head->target_len[i]);
	   	frc.setID(i, position2contig[i]);
    }


    Contig *currentContig; // = new Contig(contigSize, peMinInsert, peMaxInsert); // Contig object, memorizes all information to compute contig`s features
    int currentTid = -1;
    int reads = 0;
    uint32_t contig=0;
    uint32_t contigSize = 0;
    bam1_t *b = bam_init1();

// NOW PROCESS LIBRARIES

    if(vm.count("pe-sam")) {
        //set FRC parameters
        frc.setC_A(libraryPE.C_A);
        frc.setS_A(libraryPE.S_A);
        frc.setC_C(libraryPE.C_C);
        frc.setC_M(libraryPE.C_M);
        frc.setC_S(libraryPE.C_S);
        frc.setC_W(libraryPE.C_W);
        frc.setInsertMean(libraryPE.insertMean);
        frc.setInsertStd(libraryPE.insertStd);


    	fp = open_alignment_file(PEalignmentFile);
    	EXIT_IF_NULL(fp);
    	head = fp->header; // sam header
    	while (samread(fp, b) >= 0) {
    		//Get bam core.
    		const bam1_core_t *core = &b->core;
    		if (core == NULL) {
    			printf("Input file is corrupt!");
    			return -1;
    		}
    		++reads;

    		// new contig
    		if (is_mapped(core)) {
    			if (core->tid != currentTid) { // another contig or simply the first one
    				//cout << "now porcessing contig " << contig << "\n";
    				if(currentTid == -1) { // first read that I`m processing
    					contigSize = head->target_len[core->tid];
    					currentTid = core->tid;
    					contig = contig2position[head->target_name[currentTid]];
    					currentContig =  new Contig(contigSize, peMinInsert_recomputed, peMaxInsert_recomputed);
    				} else {
    					//cout << "CONTIG\n";
    					float coverage = frc.obtainCoverage(contig, currentContig);

    					frc.computeCEstats(currentContig, libraryPE.insertMean, libraryPE.insertMean, libraryPE.insertMean, libraryPE.insertStd);

    			    	frc.computeLowCoverageArea("PE", contig, currentContig, 1000, 200);
    			    	frc.computeHighCoverageArea("PE", contig, currentContig, 1000, 200);
    			    	frc.computeLowNormalArea("PE", contig, currentContig, 1000, 200);
    			    	frc.computeHighNormalArea("PE", contig, currentContig, 1000, 200);

    					frc.computeHighSingleArea("PE", contig, currentContig, 1000, 200);
    					frc.computeHighOutieArea("PE", contig, currentContig, 1000, 200);

    					//if(contigSize >= libraryPE.insertMean) {
   						frc.computeHighSpanningArea("PE", contig, currentContig, 1000, 200);
						frc.computeCompressionArea("PE", contig, currentContig, CEstats_PE_min, libraryPE.insertMean, libraryPE.insertMean);
						frc.computeStrechArea("PE", contig, currentContig, CEstats_PE_max, libraryPE.insertMean, libraryPE.insertMean);
							//}

    					delete currentContig; // delete hold contig
    					contigSize = head->target_len[core->tid];
    					if (contigSize < 1) {//We can't have such sizes! this can't be right
    						fprintf(stderr,"%s has size %d, which can't be right!\nCheck bam header!",head->target_name[core->tid],contigSize);
    					}
    					currentTid = core->tid; // update current identifier
    					contig = contig2position[head->target_name[currentTid]];
    					currentContig =  new Contig(contigSize, peMinInsert_recomputed, peMaxInsert_recomputed);
    					//cout << "contig " << contig << "\n";
    				}
    				currentContig->updateContig(b); // update contig with alignment
    			} else {
    				//add information to current contig
    				currentContig->updateContig(b);
    			}
    		}
    	}
    	//UPDATE LAST CONTIG
		//float coverage = frc.obtainCoverage(contig, currentContig);

    	frc.computeCEstats(currentContig, libraryPE.insertMean, libraryPE.insertMean, libraryPE.insertMean, libraryPE.insertStd);

    	frc.computeLowCoverageArea("PE", contig, currentContig, 1000, 200);
    	frc.computeHighCoverageArea("PE", contig, currentContig, 1000, 200);
    	frc.computeLowNormalArea("PE", contig, currentContig, 1000, 200);
    	frc.computeHighNormalArea("PE", contig, currentContig, 1000, 200);


		frc.computeHighSingleArea("PE", contig, currentContig, 1000, 200);
		frc.computeHighOutieArea("PE", contig, currentContig, 1000, 200);


		frc.computeHighSpanningArea("PE", contig, currentContig, 1000, 200);
		frc.computeCompressionArea("PE", contig, currentContig, CEstats_PE_min, libraryPE.insertMean, libraryPE.insertMean);
		frc.computeStrechArea("PE", contig, currentContig, CEstats_PE_max, libraryPE.insertMean, libraryPE.insertMean);


    	delete currentContig; // delete hold contig
    	samclose(fp); // close the file
    }

    string PE_CEstats = header + "_CEstats_PE.txt";
    ofstream CEstats;
    CEstats.open(PE_CEstats.c_str());

    map<float, unsigned int>::iterator it;

    map<float, unsigned int>::iterator secondIterator;



    for ( it = frc.CEstatistics.begin() ; it != frc.CEstatistics.end(); it++ ) {
    	unsigned int total = 0;
    	if( (*it).first < 0) {
    		for(secondIterator = it; secondIterator != frc.CEstatistics.begin(); secondIterator --) {
    			total += (*secondIterator).second;
    		}
    	} else {
    		for(secondIterator = it; secondIterator != frc.CEstatistics.end(); secondIterator ++) {
    			total += (*secondIterator).second;
    		}
    	}
    	CEstats << (*it).first << " " << total << endl;
 //    	CEstats << (*it).first << " " << (*it).second << endl;
    }

    CEstats.close();
    frc.CEstatistics.clear();



    featuresTotal = 0;
    for(unsigned int i=0; i< contigsNumber; i++) {
    	featuresTotal += frc.getTotal(i);
    }
    cout << "TOTAL number of features " << featuresTotal << "\n";

//NOW COMPUTE MP FEATURES
    currentTid = -1;
    reads = 0;
    contig=0;
    contigSize = 0;
    b = bam_init1();
    int ctgForCE =1;

    if(vm.count("mp-sam")) {
        //set FRC parameters
        frc.setC_A(libraryMP.C_A);
        frc.setS_A(libraryMP.S_A);
        frc.setC_C(libraryMP.C_C);
        frc.setC_M(libraryMP.C_M);
        frc.setC_S(libraryMP.C_S);
        frc.setC_W(libraryMP.C_W);
        frc.setInsertMean(libraryMP.insertMean);
        frc.setInsertStd(libraryMP.insertStd);


    	fp = open_alignment_file(MPalignmentFile);
    	EXIT_IF_NULL(fp);
    	head = fp->header; // sam header
    	while (samread(fp, b) >= 0) {
    		//Get bam core.
    		const bam1_core_t *core = &b->core;
    		if (core == NULL) {
    			printf("Input file is corrupt!");
    			return -1;
    		}
    		++reads;

    		// new contig
    		if (is_mapped(core)) {
    			if (core->tid != currentTid) { // another contig or simply the first one
    				//cout << "now porcessing contig " << contig << "\n";
    				if(currentTid == -1) { // first read that I`m processing
    					contigSize = head->target_len[core->tid];
    					currentTid = core->tid;
    					contig = contig2position[head->target_name[currentTid]];
    					currentContig =  new Contig(contigSize, mpMinInsert_recomputed, mpMaxInsert_recomputed);
    				} else {
    					//count contig features

    					float coverage = frc.obtainCoverage(contig, currentContig);


    					frc.computeCEstats(currentContig, libraryMP.insertMean, libraryMP.insertMean, libraryMP.insertMean, libraryMP.insertStd);

    					//CEstats.close();
    					//currentContig->print();
    					//if(coverage > 10) { // if mate pair library provides an enough high covereage
    					//	frc.computeLowCoverageArea("MP", contig, currentContig, 1000, 100);
    					//	frc.computeHighCoverageArea("MP", contig, currentContig,  1000, 100);
    					//}
    					//if(coverage > 10) {
    					//	frc.computeLowNormalArea("MP", contig, currentContig, 1000, 100);
    					//	frc.computeHighNormalArea("MP", contig, currentContig, 1000, 100);
    					//}



    			       	frc.computeHighSpanningArea("MP", contig, currentContig, 1000, 200);
    			       	frc.computeHighOutieArea("MP", contig, currentContig, 1000,200);
    			    	frc.computeHighSingleArea("MP", contig, currentContig, 1000, 200);

    			    	frc.computeCompressionArea("MP", contig, currentContig, CEstats_MP_min, libraryMP.insertMean, libraryMP.insertMean);
    			    	frc.computeStrechArea("MP", contig, currentContig, CEstats_MP_max, libraryMP.insertMean, libraryMP.insertMean);


    					delete currentContig; // delete hold contig
    					contigSize = head->target_len[core->tid];
    					if (contigSize < 1) {//We can't have such sizes! this can't be right
    						fprintf(stderr,"%s has size %d, which can't be right!\nCheck bam header!",head->target_name[core->tid],contigSize);
    					}
    					currentTid = core->tid; // update current identifier
    					contig = contig2position[head->target_name[currentTid]];
    					currentContig =  new Contig(contigSize, mpMinInsert_recomputed, mpMaxInsert_recomputed);
    				}
    				currentContig->updateContig(b); // update contig with alignment
    			} else {
    				//add information to current contig
    				currentContig->updateContig(b);
    			}
    		}
    	}
    	//UPDATE LAST CONTIG

		float coverage = frc.obtainCoverage(contig, currentContig);
		frc.computeCEstats(currentContig, libraryMP.insertMean, libraryMP.insertMean, libraryMP.insertMean, libraryMP.insertStd);

		//if(coverage > 10) { // if mate pair library provides an enough high covereage
		//	frc.computeLowCoverageArea("MP", contig, currentContig, 1000, 100);
		//	frc.computeHighCoverageArea("MP", contig, currentContig,  1000, 100);
		//}
		//if(coverage > 10) {
		//	frc.computeLowNormalArea("MP", contig, currentContig, 1000, 100);
		//	frc.computeHighNormalArea("MP", contig, currentContig, 1000, 100);
		//}


   		frc.computeHighSpanningArea("MP", contig, currentContig, 1000, 200);
   		frc.computeHighOutieArea("MP", contig, currentContig, 1000,200);
    	frc.computeHighSingleArea("MP", contig, currentContig, 1000, 200);

    	frc.computeCompressionArea("MP", contig, currentContig, CEstats_MP_min, libraryMP.insertMean, libraryMP.insertMean);
    	frc.computeStrechArea("MP", contig, currentContig, CEstats_MP_max, libraryMP.insertMean, libraryMP.insertMean);

    	samclose(fp); // close the file

    }



    string MP_CEstats = header + "_CEstats_MP.txt";
    CEstats.open(MP_CEstats.c_str());

    for ( it= frc.CEstatistics.begin() ; it != frc.CEstatistics.end(); it++ ) {
    	unsigned int total = 0;
    	if( (*it).first <= 0) {
    		for(secondIterator = it; secondIterator != frc.CEstatistics.begin(); secondIterator --) {
    			total += (*secondIterator).second;
    		}
        	CEstats << (*it).first << " " << total << endl;
    	} else {
    		for(secondIterator = it; secondIterator != frc.CEstatistics.end(); secondIterator ++) {
    			total += (*secondIterator).second;
    		}
    		CEstats << (*it).first << " " << total << endl;
    	}
//    	CEstats << (*it).first << " " << (*it).second << endl;
    }
    CEstats.close();
    frc.CEstatistics.clear();

    featuresTotal = 0;
    cout << "\n----------\nNow computing FRC \n------\n";

    ofstream featureOutFile;
    featureOutFile.open (featureFile.c_str());


    ofstream GFF3_features;
    string GFF3 = "Features.gff";
    GFF3_features.open(GFF3.c_str());

    GFF3_features << "##gff-version   3\n";


    for(unsigned int i=0; i< contigsNumber; i++) {
    	frc.printFeatures(i, featureOutFile);
    	frc.printFeaturesGFF3(i, GFF3_features);
    }

    frc.sortFRC();
   /* for(unsigned int i=0; i< contigsNumber; i++) {
       cout << frc.getContigLength(i) << " "; // update total number of feature seen so far
    }
    cout << "\n";
   */

    unsigned int LOW_COV_PE_features = 0;
    unsigned int HIGH_COV_PE_features = 0;
    unsigned int LOW_NORM_COV_PE_features = 0;
    unsigned int HIGH_NORM_COV_PE_features = 0;
    unsigned int HIGH_SINGLE_PE_features = 0;
    unsigned int HIGH_SPAN_PE_features = 0;
    unsigned int HIGH_OUTIE_PE_features = 0;
    unsigned int COMPR_PE_features = 0;
    unsigned int STRECH_PE_features = 0;

    unsigned int HIGH_SINGLE_MP_features = 0;
    unsigned int HIGH_OUTIE_MP_features = 0;
    unsigned int HIGH_SPAN_MP_features = 0;
    unsigned int COMPR_MP_features = 0;
    unsigned int STRECH_MP_features = 0;

    for(unsigned int i=0; i< contigsNumber; i++) {
    	featuresTotal += frc.getTotal(i); // update total number of feature seen so far

    	LOW_COV_PE_features += frc.getLOW_COV_PE(i);
    	HIGH_COV_PE_features += frc.getHIGH_COV_PE(i);
    	LOW_NORM_COV_PE_features += frc.getLOW_NORM_COV_PE(i);
    	HIGH_NORM_COV_PE_features += frc.getHIGH_NORM_COV_PE(i);
    	HIGH_SINGLE_PE_features += frc.getHIGH_SINGLE_PE(i);
    	HIGH_SPAN_PE_features += frc.getHIGH_SPAN_PE(i);
    	HIGH_OUTIE_PE_features += frc.getHIGH_OUTIE_PE(i);
    	COMPR_PE_features += frc.getCOMPR_PE(i);
    	STRECH_PE_features += frc.getSTRECH_PE(i);

    	HIGH_SINGLE_MP_features += frc.getHIGH_SINGLE_MP(i);
    	HIGH_OUTIE_MP_features += frc.getHIGH_OUTIE_MP(i);
    	HIGH_SPAN_MP_features += frc.getHIGH_SPAN_MP(i);
    	COMPR_MP_features += frc.getCOMPR_MP(i);
    	STRECH_MP_features += frc.getSTRECH_MP(i);



    }
    cout << "total number of features " << featuresTotal << "\n";



//FRCurve for all the features
    ofstream myfile;
    myfile.open (outputFile.c_str());



//    unsigned int step = ceil(featuresTotal/(float)100 + 0.5);
//   	unsigned int partial=0;
    float step = featuresTotal/(float)100;
    float partial=0;
    uint64_t edgeCoverage = 0;

    while(partial <= featuresTotal) {
    	uint32_t featuresStep = 0;
    	uint32_t contigStep    = 0;
    	featuresStep += frc.getTotal(contigStep);
    	while(featuresStep <= partial) {
    		contigStep++;
    		if(contigStep < frc.returnContigs()) {
    			featuresStep += frc.getTotal(contigStep); // CONTIG[contigStep].TOTAL
    		} else {// I read all the contigs, time to to stop
    			featuresStep = partial + 1;
    		}
    		//featuresStep += frc.getTotal(contigStep); // CONTIG[contigStep].TOTAL
    	}

    	edgeCoverage = 0;
		for(unsigned int i=0; i< contigStep; i++) {
			edgeCoverage += frc.getContigLength(i);
		}
    	float coveragePartial =  100*(edgeCoverage/(float)estimatedGenomeSize);
    	myfile << partial << " " << coveragePartial << "\n";
    	partial += step;
    	if(partial > featuresTotal) {
    		partial = featuresTotal;
    	}

    	if(contigStep == frc.returnContigs()) {
    		partial = featuresTotal + 1;
    	}
    }

    myfile.close();

// FRC for each Features
    outputFile = header + "LOW_COV_PE_FRC.txt";
    printLOW_COV_PE_FRC(outputFile, LOW_COV_PE_features, estimatedGenomeSize, frc);

    outputFile = header + "HIGH_COV_PE_FRC.txt";
    printHIGH_COV_PE_FRC(outputFile, HIGH_COV_PE_features, estimatedGenomeSize, frc);

    outputFile = header + "LOW_NORM_COV_PE_FRC.txt";
    printLOW_NORM_COV_PE_FRC(outputFile, LOW_NORM_COV_PE_features, estimatedGenomeSize, frc);

    outputFile = header + "HIGH_NORM_COV_PE_FRC.txt";
    printHIGH_NORM_COV_PE_FRC(outputFile, HIGH_NORM_COV_PE_features, estimatedGenomeSize, frc);

    outputFile = header + "HIGH_SINGLE_PE_FRC.txt";
    printHIGH_SINGLE_PE_FRC(outputFile, HIGH_SINGLE_PE_features, estimatedGenomeSize, frc);

    outputFile = header + "HIGH_OUTIE_PE_FRC.txt";
    printHIGH_OUTIE_PE_FRC(outputFile, HIGH_OUTIE_PE_features, estimatedGenomeSize, frc);

    outputFile = header + "HIGH_SPAN_PE_FRC.txt";
    printHIGH_SPAN_PE_FRC(outputFile, HIGH_SPAN_PE_features, estimatedGenomeSize, frc);

    outputFile = header + "COMPR_PE_FRC.txt";
    printCOMPR_PE_FRC(outputFile, COMPR_PE_features, estimatedGenomeSize, frc);

    outputFile = header + "STRECH_PE_FRC.txt";
    printSTRECH_PE_FRC(outputFile, STRECH_PE_features, estimatedGenomeSize, frc);



    outputFile = header + "HIGH_SINGLE_MP_FRC.txt";
    printHIGH_SINGLE_MP_FRC(outputFile, HIGH_SINGLE_MP_features, estimatedGenomeSize, frc);

    outputFile = header + "HIGH_OUTIE_MP_FRC.txt";
    printHIGH_OUTIE_MP_FRC(outputFile, HIGH_OUTIE_MP_features, estimatedGenomeSize, frc);

    outputFile = header + "HIGH_SPAN_MP_FRC.txt";
    printHIGH_SPAN_MP_FRC(outputFile, HIGH_SPAN_MP_features, estimatedGenomeSize, frc);

    outputFile = header + "COMPR_MP_FRC.txt";
    printCOMPR_MP_FRC(outputFile, COMPR_MP_features, estimatedGenomeSize, frc);

    outputFile = header + "STRECH_MP_FRC.txt";
    printSTRECH_MP_FRC(outputFile, STRECH_MP_features, estimatedGenomeSize, frc);


}




void printLOW_COV_PE_FRC(string outputFile, unsigned int LOW_COV_PE_features, unsigned long estimatedGenomeSize, FRC frc){
	ofstream myfile;
	myfile.open (outputFile.c_str());
	//unsigned int step = ceil(LOW_COV_PE_features/(float)100 + 0.5);
	float step = LOW_COV_PE_features/(float)100 ;
	float partial=0;
	uint64_t edgeCoverage = 0;


	while(partial <= LOW_COV_PE_features) {
		uint32_t featuresStep = 0;
		uint32_t contigStep    = 0;
		featuresStep += frc.getLOW_COV_PE(contigStep);
		while(featuresStep <= partial) {
			contigStep++;
			if(contigStep < frc.returnContigs()) {
				featuresStep += frc.getLOW_COV_PE(contigStep); // CONTIG[contigStep].TOTAL
			} else {// I read all the contigs, time to to stop
				featuresStep = partial + 1;
			}
		}

		edgeCoverage = 0;
		for(unsigned int i=0; i< contigStep; i++) {
			edgeCoverage += frc.getContigLength(i);
		}
		float coveragePartial =  100*(edgeCoverage/(float)estimatedGenomeSize);
		myfile << partial << " " << coveragePartial << "\n";
		partial += step;
		if(partial > LOW_COV_PE_features) {
			partial = LOW_COV_PE_features;
		}
		if(contigStep == frc.returnContigs()) {
			partial = LOW_COV_PE_features + 1;
		}
	}

	myfile.close();
}
void printHIGH_COV_PE_FRC(string outputFile, unsigned int HIGH_COV_PE_features, unsigned long estimatedGenomeSize, FRC frc) {
	ofstream myfile;
	myfile.open (outputFile.c_str());
	//unsigned int step = ceil(HIGH_COV_PE_features/(float)100 + 0.5);
	float step = HIGH_COV_PE_features/(float)100;
	float partial=0;
	uint64_t edgeCoverage = 0;

	while(partial <= HIGH_COV_PE_features) {
		uint32_t featuresStep = 0;
		uint32_t contigStep    = 0;
		featuresStep += frc.getHIGH_COV_PE(contigStep);
		while(featuresStep <= partial) {
			contigStep++;
			if(contigStep < frc.returnContigs()) {
				featuresStep += frc.getHIGH_COV_PE(contigStep); // CONTIG[contigStep].TOTAL
			} else {// I read all the contigs, time to to stop
				featuresStep = partial + 1;
			}
		}

		edgeCoverage = 0;
		for(unsigned int i=0; i< contigStep; i++) {
			edgeCoverage += frc.getContigLength(i);
		}
		float coveragePartial =  100*(edgeCoverage/(float)estimatedGenomeSize);
		myfile << partial << " " << coveragePartial << "\n";
		partial += step;
		if(partial > HIGH_COV_PE_features) {
			partial = HIGH_COV_PE_features;
		}
		if(contigStep == frc.returnContigs()) {
			partial = HIGH_COV_PE_features + 1;
		}
	}

	myfile.close();
}

void printLOW_NORM_COV_PE_FRC(string outputFile, unsigned int LOW_NORM_COV_PE_features, unsigned long estimatedGenomeSize, FRC frc) {

	ofstream myfile;
	myfile.open (outputFile.c_str());
	//unsigned int step = ceil(LOW_NORM_COV_PE_features/(float)100 + 0.5);
	float step = LOW_NORM_COV_PE_features/(float)100;
	float partial=0;
	uint64_t edgeCoverage = 0;

	while(partial <= LOW_NORM_COV_PE_features) {
		uint32_t featuresStep = 0;
		uint32_t contigStep    = 0;
		featuresStep += frc.getLOW_NORM_COV_PE(contigStep);
		while(featuresStep <= partial) {
			contigStep++;
			if(contigStep < frc.returnContigs()) {
				featuresStep += frc.getLOW_NORM_COV_PE(contigStep); // CONTIG[contigStep].TOTAL
			} else {// I read all the contigs, time to to stop
				featuresStep = partial + 1;
			}

		}

		edgeCoverage = 0;
		for(unsigned int i=0; i< contigStep; i++) {
			edgeCoverage += frc.getContigLength(i);
		}
		float coveragePartial =  100*(edgeCoverage/(float)estimatedGenomeSize);
		myfile << partial << " " << coveragePartial << "\n";
		partial += step;
		if(partial > LOW_NORM_COV_PE_features) {
			partial = LOW_NORM_COV_PE_features;
		}

		if(contigStep == frc.returnContigs()) {
			partial = LOW_NORM_COV_PE_features + 1;
		}
	}

	myfile.close();
}

void printHIGH_NORM_COV_PE_FRC(string outputFile, unsigned int HIGH_NORM_COV_PE_features, unsigned long estimatedGenomeSize, FRC frc) {
	ofstream myfile;
	myfile.open (outputFile.c_str());
	//unsigned int step = ceil(HIGH_NORM_COV_PE_features/(float)100 + 0.5);
	float step = HIGH_NORM_COV_PE_features/(float)100;
	float partial=0;

	uint64_t edgeCoverage = 0;

	while(partial <= HIGH_NORM_COV_PE_features) {
		uint32_t featuresStep = 0;
		uint32_t contigStep    = 0;
		featuresStep += frc.getHIGH_NORM_COV_PE(contigStep);
		while(featuresStep <= partial) {
			contigStep++;
			if(contigStep < frc.returnContigs()) {
				featuresStep += frc.getHIGH_NORM_COV_PE(contigStep); // CONTIG[contigStep].TOTAL
			} else {// I read all the contigs, time to to stop
				featuresStep = partial + 1;
			}

		}

		edgeCoverage = 0;
		for(unsigned int i=0; i< contigStep; i++) {
			edgeCoverage += frc.getContigLength(i);
		}
		float coveragePartial =  100*(edgeCoverage/(float)estimatedGenomeSize);
		myfile << partial << " " << coveragePartial << "\n";
		partial += step;
		if(partial > HIGH_NORM_COV_PE_features) {
			partial = HIGH_NORM_COV_PE_features;
		}

		if(contigStep == frc.returnContigs()) {
			partial = HIGH_NORM_COV_PE_features + 1;
		}
	}

	myfile.close();
}
void printHIGH_SINGLE_PE_FRC(string outputFile, unsigned int HIGH_SINGLE_PE_features, unsigned long estimatedGenomeSize, FRC frc) {
	ofstream myfile;
	myfile.open (outputFile.c_str());
	//unsigned int step = ceil(HIGH_SINGLE_PE_features/(float)100 + 0.5);
	float step = HIGH_SINGLE_PE_features/(float)100;
	float partial=0;
	uint64_t edgeCoverage = 0;

	while(partial <= HIGH_SINGLE_PE_features) {
		uint32_t featuresStep = 0;
		uint32_t contigStep    = 0;
		featuresStep += frc.getHIGH_SINGLE_PE(contigStep);
		while(featuresStep <= partial) {
			contigStep++;
			if(contigStep < frc.returnContigs()) {
				featuresStep += frc.getHIGH_SINGLE_PE(contigStep); // CONTIG[contigStep].TOTAL
			} else {// I read all the contigs, time to to stop
				featuresStep = partial + 1;
			}
		}

		edgeCoverage = 0;
		for(unsigned int i=0; i< contigStep; i++) {
			edgeCoverage += frc.getContigLength(i);
		}
		float coveragePartial =  100*(edgeCoverage/(float)estimatedGenomeSize);
		myfile << partial << " " << coveragePartial << "\n";
		partial += step;
		if(partial > HIGH_SINGLE_PE_features) {
			partial = HIGH_SINGLE_PE_features;
		}

		if(contigStep == frc.returnContigs()) {
			partial = HIGH_SINGLE_PE_features + 1;
		}
	}

	myfile.close();
}
void printHIGH_OUTIE_PE_FRC(string outputFile, unsigned int HIGH_OUTIE_PE_features, unsigned long estimatedGenomeSize, FRC frc){
	ofstream myfile;
	myfile.open (outputFile.c_str());
	//unsigned int step = ceil(HIGH_OUTIE_PE_features/(float)100 + 0.5);
	float step = HIGH_OUTIE_PE_features/(float)100;
	float partial=0;
	uint64_t edgeCoverage = 0;

	while(partial <= HIGH_OUTIE_PE_features) {
		uint32_t featuresStep = 0;
		uint32_t contigStep    = 0;
		featuresStep += frc.getHIGH_OUTIE_PE(contigStep);
		while(featuresStep <= partial) {
			contigStep++;
			if(contigStep < frc.returnContigs()) {
				featuresStep += frc.getHIGH_OUTIE_PE(contigStep); // CONTIG[contigStep].TOTAL
			} else {// I read all the contigs, time to to stop
				featuresStep = partial + 1;
			}
		}

		edgeCoverage = 0;
		for(unsigned int i=0; i< contigStep; i++) {
			edgeCoverage += frc.getContigLength(i);
		}
		float coveragePartial =  100*(edgeCoverage/(float)estimatedGenomeSize);
		myfile << partial << " " << coveragePartial << "\n";
		partial += step;
		if(partial > HIGH_OUTIE_PE_features) {
			partial = HIGH_OUTIE_PE_features;
		}

		if(contigStep == frc.returnContigs()) {
			partial = HIGH_OUTIE_PE_features + 1;
		}
	}

	myfile.close();
}
void printHIGH_SPAN_PE_FRC(string outputFile, unsigned int HIGH_SPAN_PE_features, unsigned long estimatedGenomeSize, FRC frc){
	ofstream myfile;
	myfile.open (outputFile.c_str());
	//unsigned int step = ceil(HIGH_SPAN_PE_features/(float)100 + 0.5);
	float step = HIGH_SPAN_PE_features/(float)100;
	float partial=0;
	uint64_t edgeCoverage = 0;

	while(partial <= HIGH_SPAN_PE_features) {
		uint32_t featuresStep = 0;
		uint32_t contigStep    = 0;
		featuresStep += frc.getHIGH_SPAN_PE(contigStep);
		while(featuresStep <= partial) {
			contigStep++;
			if(contigStep < frc.returnContigs()) {
				featuresStep += frc.getHIGH_SPAN_PE(contigStep); // CONTIG[contigStep].TOTAL
			} else {// I read all the contigs, time to to stop
				featuresStep = partial + 1;
			}
		}

		edgeCoverage = 0;
		for(unsigned int i=0; i< contigStep; i++) {
			edgeCoverage += frc.getContigLength(i);
		}
		float coveragePartial =  100*(edgeCoverage/(float)estimatedGenomeSize);
		myfile << partial << " " << coveragePartial << "\n";
		partial += step;
		if(partial > HIGH_SPAN_PE_features) {
			partial = HIGH_SPAN_PE_features;
		}

		if(contigStep == frc.returnContigs()) {
			partial = HIGH_SPAN_PE_features + 1;
		}
	}

	myfile.close();
}
void printCOMPR_PE_FRC(string outputFile, unsigned int COMPR_PE_features, unsigned long estimatedGenomeSize, FRC frc){
	ofstream myfile;
	myfile.open (outputFile.c_str());
	//unsigned int step = ceil(COMPR_PE_features/(float)100 + 0.5);
	float step = COMPR_PE_features/(float)100;
	float partial=0;
	uint64_t edgeCoverage = 0;

	while(partial <= COMPR_PE_features) {
		uint32_t featuresStep = 0;
		uint32_t contigStep    = 0;
		featuresStep += frc.getCOMPR_PE(contigStep);
		while(featuresStep <= partial) {
			contigStep++;
			if(contigStep < frc.returnContigs()) {
				featuresStep += frc.getCOMPR_PE(contigStep); // CONTIG[contigStep].TOTAL
			} else {// I read all the contigs, time to to stop
				featuresStep = partial + 1;
			}
		}

		edgeCoverage = 0;
		for(unsigned int i=0; i< contigStep; i++) {
			edgeCoverage += frc.getContigLength(i);
		}
		float coveragePartial =  100*(edgeCoverage/(float)estimatedGenomeSize);
		myfile << partial << " " << coveragePartial << "\n";
		partial += step;
		if(partial > COMPR_PE_features) {
			partial = COMPR_PE_features;
		}

		if(contigStep == frc.returnContigs()) {
			partial = COMPR_PE_features + 1;
		}
	}

	myfile.close();
}
void printSTRECH_PE_FRC(string outputFile, unsigned int STRECH_PE_features, unsigned long estimatedGenomeSize, FRC frc){
	ofstream myfile;
	myfile.open (outputFile.c_str());
	//unsigned int step = ceil(STRECH_PE_features/(float)100 + 0.5);
	float step = STRECH_PE_features/(float)100;
	float  partial=0;
	uint64_t edgeCoverage = 0;

	while(partial <= STRECH_PE_features) {
		uint32_t featuresStep = 0;
		uint32_t contigStep    = 0;
		featuresStep += frc.getSTRECH_PE(contigStep);
		while(featuresStep <= partial) {
			contigStep++;
			if(contigStep < frc.returnContigs()) {
				featuresStep += frc.getSTRECH_PE(contigStep); // CONTIG[contigStep].TOTAL
			} else {// I read all the contigs, time to to stop
				featuresStep = partial + 1;
			}
		}

		edgeCoverage = 0;
		for(unsigned int i=0; i< contigStep; i++) {
			edgeCoverage += frc.getContigLength(i);
		}
		float coveragePartial =  100*(edgeCoverage/(float)estimatedGenomeSize);
		myfile << partial << " " << coveragePartial << "\n";
		partial += step;
		if(partial > STRECH_PE_features) {
			partial = STRECH_PE_features;
		}
		if(contigStep == frc.returnContigs()) {
			partial = STRECH_PE_features + 1;
		}
	}

	myfile.close();
}

void printHIGH_SINGLE_MP_FRC(string outputFile, unsigned int HIGH_SINGLE_MP_features, unsigned long estimatedGenomeSize, FRC frc){
	ofstream myfile;
	myfile.open (outputFile.c_str());
	//unsigned int step = ceil(HIGH_SINGLE_MP_features/(float)100 + 0.5);
	float step = HIGH_SINGLE_MP_features/(float)100;
	float  partial=0;
	uint64_t edgeCoverage = 0;

	while(partial <= HIGH_SINGLE_MP_features) {
		uint32_t featuresStep = 0;
		uint32_t contigStep    = 0;
		featuresStep += frc.getHIGH_SINGLE_MP(contigStep);
		while(featuresStep <= partial) {
			contigStep++;
			if(contigStep < frc.returnContigs()) {
				featuresStep += frc.getHIGH_SINGLE_MP(contigStep); // CONTIG[contigStep].TOTAL
			} else {// I read all the contigs, time to to stop
				featuresStep = partial + 1;
			}
		}

		edgeCoverage = 0;
		for(unsigned int i=0; i< contigStep; i++) {
			edgeCoverage += frc.getContigLength(i);
		}
		float coveragePartial =  100*(edgeCoverage/(float)estimatedGenomeSize);
		myfile << partial << " " << coveragePartial << "\n";
		partial += step;
		if(partial > HIGH_SINGLE_MP_features) {
			partial = HIGH_SINGLE_MP_features;
		}

		if(contigStep == frc.returnContigs()) {
			partial = HIGH_SINGLE_MP_features + 1;
		}
	}

	myfile.close();
}
void printHIGH_OUTIE_MP_FRC(string outputFile, unsigned int HIGH_OUTIE_MP_features, unsigned long estimatedGenomeSize, FRC frc){
	ofstream myfile;
	myfile.open (outputFile.c_str());
	//unsigned int step = ceil(HIGH_OUTIE_MP_features/(float)100 + 0.5);
	float step = HIGH_OUTIE_MP_features/(float)100;
	float  partial=0;

	uint64_t edgeCoverage = 0;

	while(partial <= HIGH_OUTIE_MP_features) {
		uint32_t featuresStep = 0;
		uint32_t contigStep    = 0;
		featuresStep += frc.getHIGH_OUTIE_MP(contigStep);
		while(featuresStep <= partial) {
			contigStep++;
			if(contigStep < frc.returnContigs()) {
				featuresStep += frc.getHIGH_OUTIE_MP(contigStep); // CONTIG[contigStep].TOTAL
			} else {// I read all the contigs, time to to stop
				featuresStep = partial + 1;
			}

		}

		edgeCoverage = 0;
		for(unsigned int i=0; i< contigStep; i++) {
			edgeCoverage += frc.getContigLength(i);
		}
		float coveragePartial =  100*(edgeCoverage/(float)estimatedGenomeSize);
		myfile << partial << " " << coveragePartial << "\n";
		partial += step;
		if(partial > HIGH_OUTIE_MP_features) {
			partial = HIGH_OUTIE_MP_features;
		}

		if(contigStep == frc.returnContigs()) {
			partial = HIGH_OUTIE_MP_features + 1;
		}
	}

	myfile.close();
}
void printHIGH_SPAN_MP_FRC(string outputFile, unsigned int HIGH_SPAN_MP_features, unsigned long estimatedGenomeSize, FRC frc){
	ofstream myfile;
	myfile.open (outputFile.c_str());
	//unsigned int step = ceil(HIGH_SPAN_MP_features/(float)100 + 0.5);
	float step = HIGH_SPAN_MP_features/(float)100;
	float partial=0;
	uint64_t edgeCoverage = 0;

	while(partial <= HIGH_SPAN_MP_features) {
		uint32_t featuresStep = 0;
		uint32_t contigStep    = 0;
		featuresStep += frc.getHIGH_SPAN_MP(contigStep);
		while(featuresStep <= partial) {
			contigStep++;
			if(contigStep < frc.returnContigs()) {
				featuresStep += frc.getHIGH_SPAN_MP(contigStep); // CONTIG[contigStep].TOTAL
			} else {// I read all the contigs, time to to stop
				featuresStep = partial + 1;
			}
		}

		edgeCoverage = 0;
		for(unsigned int i=0; i< contigStep; i++) {
			edgeCoverage += frc.getContigLength(i);
		}
		float coveragePartial =  100*(edgeCoverage/(float)estimatedGenomeSize);
		myfile << partial << " " << coveragePartial << "\n";
		partial += step;
		if(partial > HIGH_SPAN_MP_features) {
			partial = HIGH_SPAN_MP_features;
		}

		if(contigStep == frc.returnContigs()) {
			partial = HIGH_SPAN_MP_features + 1;
		}
	}

	myfile.close();
}
void printCOMPR_MP_FRC(string outputFile, unsigned int COMPR_MP_features, unsigned long estimatedGenomeSize, FRC frc){
	ofstream myfile;
	myfile.open (outputFile.c_str());
	//unsigned int step = ceil(COMPR_MP_features/(float)100 + 0.5);
	float step = COMPR_MP_features/(float)100;
	float partial=0;
	uint64_t edgeCoverage = 0;

	while(partial <= COMPR_MP_features) {
		uint32_t featuresStep = 0;
		uint32_t contigStep    = 0;
		featuresStep += frc.getCOMPR_MP(contigStep);
		while(featuresStep <= partial) {
			contigStep++;
			if(contigStep < frc.returnContigs()) {
				featuresStep += frc.getCOMPR_MP(contigStep); // CONTIG[contigStep].TOTAL
			} else {// I read all the contigs, time to to stop
				featuresStep = partial + 1;
			}
		}

		edgeCoverage = 0;
		for(unsigned int i=0; i< contigStep; i++) {
			edgeCoverage += frc.getContigLength(i);
		}
		float coveragePartial =  100*(edgeCoverage/(float)estimatedGenomeSize);
		myfile << partial << " " << coveragePartial << "\n";
		partial += step;
		if(partial > COMPR_MP_features) {
			partial = COMPR_MP_features;
		}
		if(contigStep == frc.returnContigs()) {
			partial = COMPR_MP_features + 1;
		}

	}

	myfile.close();
}
void printSTRECH_MP_FRC(string outputFile, unsigned int STRECH_MP_features, unsigned long estimatedGenomeSize, FRC frc){
	ofstream myfile;
	myfile.open (outputFile.c_str());
	//unsigned int step = ceil(STRECH_MP_features/(float)100 + 0.5);
	float step = STRECH_MP_features/(float)100;
	float partial=0;

	uint64_t edgeCoverage = 0;

	while(partial <= STRECH_MP_features) {
		uint32_t featuresStep = 0;
		uint32_t contigStep    = 0;
		featuresStep += frc.getSTRECH_MP(contigStep);
		while(featuresStep <= partial) {
			contigStep++;
			if(contigStep < frc.returnContigs()) {
				featuresStep += frc.getSTRECH_MP(contigStep); // CONTIG[contigStep].TOTAL
			} else {// I read all the contigs, time to to stop
				featuresStep = partial + 1;
			}
		}

		edgeCoverage = 0;
		for(unsigned int i=0; i< contigStep; i++) {
			edgeCoverage += frc.getContigLength(i);
		}
		float coveragePartial =  100*(edgeCoverage/(float)estimatedGenomeSize);
		myfile << partial << " " << coveragePartial << "\n";
		partial += step;
		if(partial > STRECH_MP_features) {
			partial = STRECH_MP_features;
		}

		if(contigStep == frc.returnContigs()) {
			partial = STRECH_MP_features + 1;
		}
	}

	myfile.close();
}



LibraryStatistics computeLibraryStats(samfile_t *fp, unsigned int minInsert, unsigned int maxInsert, uint64_t genomeLength) {
	LibraryStatistics library;
	//Initialize bam entity
	bam1_t *b = bam_init1();

	//All var declarations
	unsigned int contigs = 0; // number of contigs/scaffolds
// total reads
	uint32_t reads = 0;
	uint64_t readsLength = 0;   // total length of reads
	uint32_t unmappedReads = 0;
	uint32_t mappedReads = 0;
	uint32_t zeroQualityReads = 0;
	uint32_t duplicates = 0;

	uint64_t contigSize = 0;
	uint64_t insertsLength = 0; // total inserts length
	float insertMean;
	float insertStd;

// mated reads (not necessary correctly mated)
	uint32_t matedReads = 0;        // length of reads that align on a contig with the mate
	uint64_t matedReadsLength = 0;  // total length of mated reads
 // correctly aligned mates
	uint32_t correctlyMatedReads = 0; // total number of correctly mated reads
	uint64_t correctlyMatedReadsLength = 0; // length of correctly mated reads
// wrongly oriented reads
	uint32_t wronglyOrientedReads = 0; // number of wrongly oriented reads
	uint64_t wronglyOrientedReadsLength = 0; // length of wrongly oriented reads
// wrongly distance reads
	uint32_t wronglyDistanceReads       = 0; // number of reads at the wrong distance
	uint64_t wronglyDistanceReadsLength = 0;  // total length of reads placed in different contigs
// singletons
	uint32_t singletonReads = 0; // number of singleton reads
	uint64_t singletonReadsLength = 0;     // total length of singleton reads
// mates on different contigs
	uint32_t matedDifferentContig = 0; // number of contig placed in a different contig
	uint64_t matedDifferentContigLength = 0; // total number of reads placed in different contigs

	float C_A = 0; // total read coverage
	float S_A = 0; // total span coverage
	float C_M = 0; // coverage induced by correctly aligned pairs
	float C_W = 0; // coverage induced by wrongly mated pairs
	float C_S = 0; // coverage induced by singletons
	float C_C = 0; // coverage induced by reads with mate on a diferent contif

// compute mean and std on the fly
	float Mk = 0;
	float Qk = 0;
	uint32_t counterK = 1;
//Keep header for further reference
    bam_header_t* head = fp->header;
    int32_t currentTid = -1;
    int32_t iSize;

    while (samread(fp, b) >= 0) {
	      //Get bam core.
	      const bam1_core_t *core = &b->core;
	      if (core == NULL) {  //There is something wrong with the read/file
	    	  printf("Input file is corrupt!");
	    	  return library;
	      }
	      ++reads; // otherwise one more read is readed

	      if (!is_mapped(core)) {
	    	  ++unmappedReads;
	      } else {
	    	  if (core->tid != currentTid) {
	    		  //Get length of next section
	    		  contigSize = head->target_len[core->tid];
	    		  contigs++;
	    		  if (contigSize < 1) {//We can't have such sizes! this can't be right
	    			  fprintf(stderr,"%s has size %d, which can't be right!\nCheck bam header!",head->target_name[core->tid],contigSize);
	    		  }
	    		  currentTid = core->tid;
	    	  }

	    	  if(!(core->flag&BAM_FUNMAP) && !(core->flag&BAM_FDUP) && !(core->flag&BAM_FSECONDARY) && !(core->flag&BAM_FQCFAIL)) { // if read has been mapped and it is not a DUPLICATE or a SECONDARY alignment
	    		  uint32_t* cigar = bam1_cigar(b);
	    		  ++mappedReads;
	    		  uint32_t alignmentLength = bam_cigar2qlen(core,cigar);
	    		  readsLength += alignmentLength;
	    		  uint32_t startRead = core->pos; // start position on the contig
	    		  uint32_t startPaired;
	    		  //Now check if reads belong to a proper pair: both reads aligned on the same contig at the expected distance and orientation
	    		  if ((core->flag&BAM_FREAD1) //First in pair
	    				  && !(core->flag&BAM_FMUNMAP) /*Mate is also mapped!*/
	    				  && (core->tid == core->mtid) /*Mate on the same chromosome*/
	    		  ) {
	    			  //pair is mapped on the same contig and I'm looking the first pair
	    			  startPaired = core->mpos;
	    			  if(startRead < startPaired) {
	    				  iSize = (startPaired + core->l_qseq -1) - startRead; // insert size, I consider both reads of the same length
	    				  if(!(core->flag&BAM_FREVERSE) && (core->flag&BAM_FMREVERSE) ) { //
	    					  //here reads are correctly oriented
	    					  if (minInsert <= iSize && iSize <= maxInsert) { //this is a right insert
	    						  if(counterK == 1) {
	    							  Mk = iSize;
	    							  Qk = 0;
	    							  counterK++;
	    						  } else {
	    							  float oldMk = Mk;
	    							  float oldQk = Qk;
	    							  Mk = oldMk + (iSize - oldMk)/counterK;
	    							  Qk = oldQk + (counterK-1)*(iSize - oldMk)*(iSize - oldMk)/(float)counterK;
	    							  counterK++;
	    						  }
	    						  insertsLength += iSize;
	    						  correctlyMatedReads++;
	    						  correctlyMatedReadsLength +=  bam_cigar2qlen(core,cigar); // update number of correctly mapped and their length
	    					  } else {
	    						  wronglyDistanceReads++;
	    						  wronglyDistanceReadsLength += bam_cigar2qlen(core,cigar);
	    					  }
	    				  } else {
	    					  //pair is wrongly oriented
	    					  wronglyOrientedReads++;
	    					  wronglyOrientedReadsLength += bam_cigar2qlen(core,cigar);
	    				  }
	    			  } else {
	    				  iSize = (startRead + alignmentLength - 1) - startPaired;
	    				  if((core->flag&BAM_FREVERSE) && !(core->flag&BAM_FMREVERSE) ) { //
	    					  //here reads are correctly oriented
	    					  //here reads are correctly oriented
	    					  if (minInsert <= iSize && iSize <= maxInsert) { //this is a right insert
	    						  if(counterK == 1) {
	    							  Mk = iSize;
	    							  Qk = 0;
	    							  counterK++;
	    						  } else {
	    							  float oldMk = Mk;
	    							  float oldQk = Qk;
	    							  Mk = oldMk + (iSize - oldMk)/counterK;
	    							  Qk = oldQk + (counterK-1)*(iSize - oldMk)*(iSize - oldMk)/(float)counterK;
	    							  counterK++;
	    						  }
	    						  insertsLength += iSize;
	    						  correctlyMatedReads++;
	    						  correctlyMatedReadsLength +=  bam_cigar2qlen(core,cigar); // update number of correctly mapped and their length
	    					  } else {
	    						  wronglyDistanceReads++;
	    						  wronglyDistanceReadsLength += bam_cigar2qlen(core,cigar);
	    					  }
	    				  } else {
	    					  //pair is wrongly oriented
	    					  wronglyOrientedReads++;
	    					  wronglyOrientedReadsLength += bam_cigar2qlen(core,cigar);
	    				  }
	    			  }
	    		  } else  if ((core->flag&BAM_FREAD2) //Second in pair
	    				  && !(core->flag&BAM_FMUNMAP) /*Mate is also mapped!*/
	    				  && (core->tid == core->mtid) /*Mate on the same chromosome*/
	    		  )
	    	// if I'm considering the second read in a pair I must check it is is a correctly mated read and if this is the case update the right variables
	    		  {
	    			  startPaired = core->mpos;
	    			  if(startRead > startPaired) {
	    				  iSize = (startRead + alignmentLength -1) - startPaired;
	    				  if((core->flag&BAM_FREVERSE) && !(core->flag&BAM_FMREVERSE) ) { //
	    					  //here reads are correctly oriented
	    					  if (minInsert <= iSize && iSize <= maxInsert) { //this is a right insert, no need to update insert coverage
	    						  correctlyMatedReads++;
	    						  correctlyMatedReadsLength +=  bam_cigar2qlen(core,cigar); // update number of correctly mapped and their length
	    					  } else {
	    						  wronglyDistanceReads++;
	    						  wronglyDistanceReadsLength += bam_cigar2qlen(core,cigar);
	    					  }
	    				  } else {
	    					  //pair is wrongly oriented
	    					  wronglyOrientedReads++;
	    					  wronglyOrientedReadsLength += bam_cigar2qlen(core,cigar);
	    				  }
	    			  } else {
	    				  iSize = (startPaired + core->l_qseq -1) - startRead;
	    				  if(!(core->flag&BAM_FREVERSE) && (core->flag&BAM_FMREVERSE) ) { //
	    					  //here reads are correctly oriented
	    					  if (minInsert <= iSize && iSize <= maxInsert) { //this is a right insert, no need to update insert coverage
	    						  correctlyMatedReads++;
	    						  correctlyMatedReadsLength +=  bam_cigar2qlen(core,cigar); // update number of correctly mapped and their length
	    					  }else {
	    						  wronglyDistanceReads++;
	    						  wronglyDistanceReadsLength += bam_cigar2qlen(core,cigar);
	    					  }
	    				  } else {
	    					  //pair is wrongly oriented
	    					  wronglyOrientedReads++;
	    					  wronglyOrientedReadsLength += bam_cigar2qlen(core,cigar);
	    				  }
	    			  }
	    		  } else if (core->tid != core->mtid && !(core->flag&BAM_FMUNMAP)) {
	    			  //Count inter-chrom pairs
	    			  matedDifferentContig++;
	    			  matedDifferentContigLength += bam_cigar2qlen(core,cigar);
	    		  } else if(core->flag&BAM_FMUNMAP) {
		    		  // if mate read is unmapped
	    			  singletonReads++;
	    			  singletonReadsLength =+ bam_cigar2qlen(core,cigar);
	    		  }


	    		  if (core->flag&BAM_FPROPER_PAIR) {
	    			  //Is part of a proper pair
	    			  matedReads ++; // increment number of mated reads
	    			  matedReadsLength += bam_cigar2qlen(core,cigar); // add the length of the read aligne as proper mate (not necessary correctly mated)
	    		  }

	    		  if (core->flag&BAM_FDUP) {   //This is a duplicate. Don't count it!.
	    			  ++duplicates;
	    		  }
	    	  } else {
	    		  ++zeroQualityReads;

	    	  }
	      }
    }

    cout << "LIBRARY STATISTICS\n";
    cout << "\ttotal reads number " << reads << "\n";
    cout << "\ttotal mapped reads " << mappedReads << "\n";
    cout << "\ttotal unmapped reads " << unmappedReads << "\n";
    cout << "\tproper pairs " << matedReads << "\n";
    cout << "\tzero quality reads " << zeroQualityReads << "\n";
    cout << "\tcorrectly oriented " << correctlyMatedReads << "\n";
    cout << "\twrongly oriented " << wronglyOrientedReads << "\n";
    cout << "\twrongly distance " << wronglyDistanceReads << "\n";
    cout << "\twrongly contig " <<  matedDifferentContig << "\n";
    cout << "\tsingletons " << singletonReads << "\n";

    uint32_t total = correctlyMatedReads + wronglyOrientedReads + wronglyDistanceReads + matedDifferentContig + singletonReads;
    cout << "\ttotal " << total << "\n";
    cout << "\tCoverage statistics\n";

    library.C_A = C_A = readsLength/(float)genomeLength;
    library.S_A = S_A = insertsLength/(float)genomeLength;
    library.C_M = C_M = correctlyMatedReadsLength/(float)genomeLength;
    library.C_W = C_W = (wronglyDistanceReadsLength + wronglyOrientedReadsLength)/(float)genomeLength;
    library.C_S = C_S = (singletonReadsLength)/(float)genomeLength;
    library.C_C = C_C = matedDifferentContigLength/(float)genomeLength;
    library.insertMean = insertMean = Mk;
    Qk = sqrt(Qk/counterK);
    library.insertStd = insertStd = Qk;

    cout << "\tC_A = " << C_A << endl;
    cout << "\tS_A = " << S_A << endl;
    cout << "\tC_M = " << C_M << endl;
    cout << "\tC_W = " << C_W << endl;
    cout << "\tC_S = " << C_S << endl;
    cout << "\tC_C = " << C_C << endl;
    cout << "\tMean Insert length = " << Mk << endl;
    cout << "\tStd Insert length = " << Qk << endl;
    cout << "----------\n";

    return library;


}


