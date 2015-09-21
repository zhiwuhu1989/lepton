#include <assert.h>
#include <fstream>
#include <iostream>

#include "model.hh"
#include "mmap.hh"
int32_t ProbabilityTablesBase::icos_idct_edge_8192_dequantized_x_[3][64] __attribute__ ((aligned (16))) = {{0}};

int32_t ProbabilityTablesBase::icos_idct_edge_8192_dequantized_y_[3][64] __attribute__ ((aligned (16))) = {{0}};
Model ProbabilityTablesBase::model_;
int32_t ProbabilityTablesBase::icos_idct_linear_8192_dequantized_[3][64] __attribute__ ((aligned (16))) = {{0}};
#ifdef ANNOTATION_ENABLED
Context *gctx = (Context*)memset(calloc(sizeof(Context),1), 0xff, sizeof(Context));
#endif

uint16_t ProbabilityTablesBase::quantization_table_[3][64] __attribute__ ((aligned(16)));

uint16_t ProbabilityTablesBase::freqmax_[3][64] __attribute__ ((aligned (16)));

uint8_t ProbabilityTablesBase::min_noise_threshold_[3][64] __attribute__ ((aligned (16)));

uint8_t ProbabilityTablesBase::bitlen_freqmax_[3][64] __attribute__ ((aligned (16)));

void serialize_model(const Model & model, std::ofstream & output )
{
  output.write( reinterpret_cast<const char*>( &model ), sizeof( model ) );
}

void optimize_model(Model &model)
{
  //model.forall( [&] ( Branch & x ) { x.optimize(); } );
}


bool filter(const Branch& a,
            const Branch* b) {
#ifndef USE_COUNT_FREE_UPDATE
    if (a.true_count() == 0 && a.false_count() == 0) {
        return false;
    }
    if (b) {
        if (a.prob() + 1 == b->prob() ||
            a.prob() == b->prob() + 1 ||
            a.prob() == b->prob()) {
            return false;
        }
    } else {
        return a.true_count () > 300 && a.false_count() > 300;
    }
#endif
    return true;
}
template<class BranchArray> void print_helper(const BranchArray& ba,
                                              const BranchArray* other,
                                              const std::string &table_name,
                                              const std::vector<std::string> &names,
                                              std::vector<uint32_t> &values,
                                              Model::PrintabilitySpecification print_branch_bitmask) {
    values.push_back(0);
    for (size_t i = 0; i < ba.dimsize(); ++i) {
        values.back() = i;
        auto subarray = ba.at(i);
        auto otherarray = &subarray;
        otherarray= nullptr;
        print_helper(subarray, otherarray, table_name, names, values, print_branch_bitmask);
    }
    values.pop_back();
}

bool is_printable(uint64_t true_count, uint64_t false_count,
                  double true_false_ratio, double other_ratio, bool other,
                  Model::PrintabilitySpecification spec) {
    if (other) {
        if (true_count + false_count >= spec.min_samples) {
            double delta = true_false_ratio - other_ratio;
            if (delta < 0) delta = -delta;
            if (delta < spec.tolerance) {
                return (Model::CLOSE_TO_ONE_ANOTHER & spec.printability_bitmask) ? true : false;
            } else {
                return (Model::PRINTABLE_OK & spec.printability_bitmask) ? true : false;
            }
        } else {
            return (Model::PRINTABLE_INSIGNIFICANT & spec.printability_bitmask) ? true : false;
        }
    } else {
        if (true_count + false_count >= spec.min_samples) {
            double delta = true_false_ratio - .5;
            if (delta < 0) delta = -delta;
            if (delta < spec.tolerance) {
                return (Model::CLOSE_TO_50 & spec.printability_bitmask) ? true : false;
            } else {
                return (Model::PRINTABLE_OK & spec.printability_bitmask) ? true : false;
            }
        } else {
            return (Model::PRINTABLE_INSIGNIFICANT & spec.printability_bitmask) ? true : false;
        }
    }
}
template<> void print_helper(const Branch& ba,
                             const Branch* other,
                             const std::string&table_name,
                             const std::vector<std::string> &names,
                             std::vector<uint32_t> &values,
                             Model::PrintabilitySpecification print_branch_bitmask) {
#ifndef USE_COUNT_FREE_UPDATE
    double ratio = (ba.true_count() + 1) / (double)(ba.false_count() + ba.true_count() + 2);
    (void) ratio;
    double other_ratio = ratio;
    if (other) {
        other_ratio = (other->true_count() + 1) / (double)(other->false_count() + other->true_count() + 2);
    }
    (void) other_ratio;
    if (ba.true_count() > 0 ||  ba.false_count() > 1) {
        if (is_printable(ba.true_count(), ba.false_count(), ratio, other_ratio, !!other, print_branch_bitmask))
        {
            assert(names.size() == values.size());
            std::cout <<table_name<<"::";
            for (size_t i = 0; i < names.size(); ++i) {
                std::cout << names[i]<<'['<<values[i]<<']';
            }
            std::cout << " = (" << ba.true_count() <<", "<<  (ba.false_count() - 1) << ")";
            if (other) {
                std::cout << " = (" << other->true_count() <<", "<<  (other->false_count() - 1) << "}";
            }
            std::cout << std::endl;
        }
    }
#endif
}
template<class BranchArray> void print_all(const BranchArray &ba,
                                           const BranchArray *other_ba,
                                           const std::string &table_name,
                                           const std::vector<std::string> &names,
                                           Model::PrintabilitySpecification spec) {
    std::vector<uint32_t> tmp;
    print_helper(ba, other_ba, table_name, names, tmp, spec);
}

const Model &Model::debug_print(const Model * other,
                                                        Model::PrintabilitySpecification spec)const
{
    print_all(this->num_nonzeros_counts_7x7_,
              other ? &other->num_nonzeros_counts_7x7_ : nullptr,
              "NONZERO 7x7",
              {"cmp","nbr","bit","prevbits"}, spec);
    
    print_all(this->num_nonzeros_counts_1x8_,
              other ? &other->num_nonzeros_counts_1x8_ : nullptr,
              "NONZERO_1x8",
              {"cmp","eobx","num_nonzeros","bit","prevbits"}, spec);
    print_all(this->num_nonzeros_counts_8x1_,
              other ? &other->num_nonzeros_counts_8x1_ : nullptr,
              "NONZERO_8x1",
              {"cmp","eobx","num_nonzeros","bit","prevbits"}, spec);
    print_all(this->exponent_counts_dc_,
              other ? &other->exponent_counts_dc_ : nullptr,
              "EXP_DC",
              {"cmp","num_nonzeros","neigh_exp","bit","prevbits"}, spec);
    print_all(this->exponent_counts_,
              other ? &other->exponent_counts_ : nullptr,
              "EXP7x7",
              {"cmp","coef","num_nonzeros","neigh_exp","bit","prevbits"}, spec);
    print_all(this->exponent_counts_x_,
              other ? &other->exponent_counts_x_: nullptr,
              "EXP_8x1",
              {"cmp","coef","num_nonzeros","neigh_exp","bit","prevbits"}, spec);
    print_all(this->residual_noise_counts_,
              other ? &other->residual_noise_counts_: nullptr,
              "NOISE",
              {"cmp","coef","num_nonzeros","bit"}, spec);
    print_all(this->residual_threshold_counts_,
              other ? &other->residual_threshold_counts_ : nullptr,
              "THRESH8",
              {"cmp","max","exp","prevbits"}, spec);
    print_all(this->sign_counts_,
              other ? &other->sign_counts_ : nullptr,
              "SIGN",
              {"cmp","lakh","exp"}, spec);
    
    return *this;
}

void normalize_model(Model& model) {
    model.forall( [&] ( Branch & x ) { x.normalize(); } );
}

void ProbabilityTablesBase::load_probability_tables()
{
    const char * model_name = getenv( "LEPTON_COMPRESSION_MODEL" );
    if ( not model_name ) {
        std::cerr << "Using default (bad!) probability tables!" << std::endl;
    } else {
        MMapFile model_file { model_name };
        ProbabilityTables<false, false, false, BlockType::Y> model_tables(BlockType::Y);
        model_tables.load(model_file.slice());
        model_tables.normalize();
    }
}

void reset_model(Model&model)
{
    model.forall( [&] ( Branch & x ) { x = Branch(); } );
}


void load_model(Model&model, const Slice & slice )
{
    const size_t expected_size = sizeof( model );
    (void)expected_size;
    assert(slice.size() == expected_size && "unexpected model file size.");
    
    memcpy( &model, slice.buffer(), slice.size() );
}
static Branch::ProbUpdate* load_update_lookup_patch() {
    FILE * fp = fopen("lookup_patch.txt", "r");
    if (fp) {
    const int LMAX = 256;
    char line[LMAX + 1] = {};
    while (fgets(line, LMAX, fp)) {
        int count = 0, index = -1, delta_prob = 0, delta_false_log_prob = 0, delta_true_log_prob = 0;
        int ret = sscanf(line, "%d %d %d %d %d", &count, &index, &delta_prob, &delta_false_log_prob, &delta_true_log_prob);
        assert(ret == 5);
        if (index < 256 && index >= 0) {
            Branch::update_lookup[count][index].prob += delta_prob;
            Branch::update_lookup[count][index].next[0].log_prob += delta_false_log_prob;
            Branch::update_lookup[count][index].next[1].log_prob += delta_true_log_prob;
        }
    }
    }
    return Branch::update_lookup[0];
}
Branch::ProbUpdate* nop = load_update_lookup_patch();
Branch::ProbUpdate Branch::update_lookup[4][256] = {
{
    {0x0,{{0x47}, {0x8}}},
    {0x0,{{0x47}, {0x8}}},
    {0x0,{{0x47}, {0x8}}},
    {0x0,{{0x47}, {0x8}}},
    {0x0,{{0x47}, {0x8}}},
    {0x0,{{0x47}, {0x8}}},
    {0x0,{{0x48}, {0x8}}},
    {0x0,{{0x48}, {0x8}}},
    {0x0,{{0x48}, {0x8}}},
    {0x0,{{0x48}, {0x8}}},
    {0x0,{{0x48}, {0x8}}},
    {0x0,{{0x48}, {0x8}}},
    {0x0,{{0x48}, {0x9}}},
    {0x0,{{0x48}, {0xa}}},
    {0x0,{{0x48}, {0xb}}},
    {0x0,{{0x48}, {0xc}}},
    {0x1,{{0x48}, {0xd}}},
    {0x1,{{0x48}, {0xe}}},
    {0x1,{{0x48}, {0xf}}},
    {0x1,{{0x49}, {0x10}}},
    {0x1,{{0x49}, {0x11}}},
    {0x1,{{0x49}, {0x12}}},
    {0x1,{{0x49}, {0x13}}},
    {0x1,{{0x49}, {0x14}}},
    {0x1,{{0x49}, {0x15}}},
    {0x1,{{0x49}, {0x16}}},
    {0x1,{{0x49}, {0x17}}},
    {0x1,{{0x49}, {0x18}}},
    {0x1,{{0x4a}, {0x19}}},
    {0x1,{{0x4a}, {0x1a}}},
    {0x1,{{0x4a}, {0x1b}}},
    {0x1,{{0x4a}, {0x1c}}},
    {0x2,{{0x4a}, {0x1e}}},
    {0x2,{{0x4a}, {0x1f}}},
    {0x2,{{0x4a}, {0x20}}},
    {0x2,{{0x4b}, {0x21}}},
    {0x2,{{0x4b}, {0x22}}},
    {0x2,{{0x4b}, {0x23}}},
    {0x2,{{0x4b}, {0x24}}},
    {0x2,{{0x4b}, {0x25}}},
    {0x2,{{0x4c}, {0x26}}},
    {0x3,{{0x4c}, {0x27}}},
    {0x3,{{0x4c}, {0x28}}},
    {0x3,{{0x4c}, {0x29}}},
    {0x3,{{0x4c}, {0x2a}}},
    {0x3,{{0x4d}, {0x2b}}},
    {0x3,{{0x4d}, {0x2c}}},
    {0x3,{{0x4d}, {0x2d}}},
    {0x4,{{0x4d}, {0x2e}}},
    {0x4,{{0x4e}, {0x2f}}},
    {0x4,{{0x4e}, {0x30}}},
    {0x4,{{0x4e}, {0x31}}},
    {0x4,{{0x4f}, {0x32}}},
    {0x5,{{0x4f}, {0x33}}},
    {0x5,{{0x4f}, {0x34}}},
    {0x5,{{0x4f}, {0x35}}},
    {0x5,{{0x50}, {0x36}}},
    {0x6,{{0x50}, {0x37}}},
    {0x6,{{0x51}, {0x38}}},
    {0x6,{{0x51}, {0x39}}},
    {0x6,{{0x51}, {0x3a}}},
    {0x7,{{0x52}, {0x3b}}},
    {0x7,{{0x52}, {0x3c}}},
    {0x7,{{0x52}, {0x3d}}},
    {0x8,{{0x53}, {0x3e}}},
    {0x8,{{0x53}, {0x3f}}},
    {0x9,{{0x54}, {0x40}}},
    {0x9,{{0x54}, {0x41}}},
    {0x9,{{0x55}, {0x42}}},
    {0xa,{{0x55}, {0x43}}},
    {0xa,{{0x56}, {0x44}}},
    {0xb,{{0x56}, {0x45}}},
    {0xb,{{0x57}, {0x46}}},
    {0xc,{{0x57}, {0x47}}},
    {0xc,{{0x58}, {0x48}}},
    {0xd,{{0x58}, {0x49}}},
    {0xd,{{0x59}, {0x4a}}},
    {0xe,{{0x59}, {0x4b}}},
    {0xf,{{0x5a}, {0x4c}}},
    {0xf,{{0x5a}, {0x4d}}},
    {0x10,{{0x5b}, {0x4e}}},
    {0x11,{{0x5c}, {0x4f}}},
    {0x12,{{0x5c}, {0x50}}},
    {0x12,{{0x5d}, {0x51}}},
    {0x13,{{0x5d}, {0x52}}},
    {0x14,{{0x5e}, {0x53}}},
    {0x15,{{0x5f}, {0x54}}},
    {0x16,{{0x5f}, {0x55}}},
    {0x17,{{0x60}, {0x57}}},
    {0x18,{{0x61}, {0x58}}},
    {0x19,{{0x61}, {0x59}}},
    {0x1a,{{0x62}, {0x5a}}},
    {0x1b,{{0x63}, {0x5b}}},
    {0x1d,{{0x64}, {0x5c}}},
    {0x1e,{{0x64}, {0x5d}}},
    {0x1f,{{0x65}, {0x5e}}},
    {0x21,{{0x66}, {0x5f}}},
    {0x22,{{0x67}, {0x60}}},
    {0x24,{{0x67}, {0x61}}},
    {0x25,{{0x68}, {0x62}}},
    {0x27,{{0x69}, {0x63}}},
    {0x29,{{0x6a}, {0x64}}},
    {0x2b,{{0x6a}, {0x65}}},
    {0x2d,{{0x6b}, {0x66}}},
    {0x2f,{{0x6c}, {0x67}}},
    {0x31,{{0x6d}, {0x68}}},
    {0x33,{{0x6e}, {0x69}}},
    {0x35,{{0x6e}, {0x6a}}},
    {0x38,{{0x6f}, {0x6b}}},
    {0x3a,{{0x70}, {0x6c}}},
    {0x3d,{{0x71}, {0x6d}}},
    {0x3f,{{0x72}, {0x6e}}},
    {0x42,{{0x73}, {0x6f}}},
    {0x45,{{0x73}, {0x70}}},
    {0x48,{{0x74}, {0x71}}},
    {0x4c,{{0x75}, {0x72}}},
    {0x4f,{{0x76}, {0x73}}},
    {0x52,{{0x77}, {0x74}}},
    {0x56,{{0x78}, {0x75}}},
    {0x5a,{{0x79}, {0x76}}},
    {0x5e,{{0x7a}, {0x77}}},
    {0x62,{{0x7b}, {0x78}}},
    {0x67,{{0x7b}, {0x79}}},
    {0x6b,{{0x7c}, {0x7a}}},
    {0x70,{{0x7d}, {0x7b}}},
    {0x75,{{0x7e}, {0x7c}}},
    {0x7a,{{0x7f}, {0x7d}}},
    {0x80,{{0x80}, {0x7e}}},
    {0x7f,{{0x81}, {0x7f}}},
    {0x85,{{0x82}, {0x80}}},
    {0x8a,{{0x83}, {0x81}}},
    {0x8f,{{0x84}, {0x82}}},
    {0x94,{{0x85}, {0x83}}},
    {0x98,{{0x86}, {0x84}}},
    {0x9d,{{0x87}, {0x84}}},
    {0xa1,{{0x88}, {0x85}}},
    {0xa5,{{0x89}, {0x86}}},
    {0xa9,{{0x8a}, {0x87}}},
    {0xad,{{0x8b}, {0x88}}},
    {0xb0,{{0x8c}, {0x89}}},
    {0xb3,{{0x8d}, {0x8a}}},
    {0xb7,{{0x8e}, {0x8b}}},
    {0xba,{{0x8f}, {0x8c}}},
    {0xbd,{{0x90}, {0x8c}}},
    {0xc0,{{0x91}, {0x8d}}},
    {0xc2,{{0x92}, {0x8e}}},
    {0xc5,{{0x93}, {0x8f}}},
    {0xc7,{{0x94}, {0x90}}},
    {0xca,{{0x95}, {0x91}}},
    {0xcc,{{0x96}, {0x91}}},
    {0xce,{{0x97}, {0x92}}},
    {0xd0,{{0x98}, {0x93}}},
    {0xd2,{{0x99}, {0x94}}},
    {0xd4,{{0x9a}, {0x95}}},
    {0xd6,{{0x9b}, {0x95}}},
    {0xd8,{{0x9c}, {0x96}}},
    {0xda,{{0x9d}, {0x97}}},
    {0xdb,{{0x9e}, {0x98}}},
    {0xdd,{{0x9f}, {0x98}}},
    {0xde,{{0xa0}, {0x99}}},
    {0xe0,{{0xa1}, {0x9a}}},
    {0xe1,{{0xa2}, {0x9b}}},
    {0xe2,{{0xa3}, {0x9b}}},
    {0xe4,{{0xa4}, {0x9c}}},
    {0xe5,{{0xa5}, {0x9d}}},
    {0xe6,{{0xa6}, {0x9e}}},
    {0xe7,{{0xa7}, {0x9e}}},
    {0xe8,{{0xa8}, {0x9f}}},
    {0xe9,{{0xaa}, {0xa0}}},
    {0xea,{{0xab}, {0xa0}}},
    {0xeb,{{0xac}, {0xa1}}},
    {0xec,{{0xad}, {0xa2}}},
    {0xed,{{0xae}, {0xa2}}},
    {0xed,{{0xaf}, {0xa3}}},
    {0xee,{{0xb0}, {0xa3}}},
    {0xef,{{0xb1}, {0xa4}}},
    {0xf0,{{0xb2}, {0xa5}}},
    {0xf0,{{0xb3}, {0xa5}}},
    {0xf1,{{0xb4}, {0xa6}}},
    {0xf2,{{0xb5}, {0xa6}}},
    {0xf2,{{0xb6}, {0xa7}}},
    {0xf3,{{0xb7}, {0xa7}}},
    {0xf3,{{0xb8}, {0xa8}}},
    {0xf4,{{0xb9}, {0xa8}}},
    {0xf4,{{0xba}, {0xa9}}},
    {0xf5,{{0xbb}, {0xa9}}},
    {0xf5,{{0xbc}, {0xaa}}},
    {0xf6,{{0xbd}, {0xaa}}},
    {0xf6,{{0xbe}, {0xab}}},
    {0xf6,{{0xbf}, {0xab}}},
    {0xf7,{{0xc0}, {0xac}}},
    {0xf7,{{0xc1}, {0xac}}},
    {0xf8,{{0xc2}, {0xad}}},
    {0xf8,{{0xc3}, {0xad}}},
    {0xf8,{{0xc4}, {0xad}}},
    {0xf9,{{0xc5}, {0xae}}},
    {0xf9,{{0xc6}, {0xae}}},
    {0xf9,{{0xc7}, {0xae}}},
    {0xf9,{{0xc8}, {0xaf}}},
    {0xfa,{{0xc9}, {0xaf}}},
    {0xfa,{{0xca}, {0xb0}}},
    {0xfa,{{0xcb}, {0xb0}}},
    {0xfa,{{0xcc}, {0xb0}}},
    {0xfb,{{0xcd}, {0xb0}}},
    {0xfb,{{0xce}, {0xb1}}},
    {0xfb,{{0xcf}, {0xb1}}},
    {0xfb,{{0xd0}, {0xb1}}},
    {0xfb,{{0xd1}, {0xb2}}},
    {0xfc,{{0xd2}, {0xb2}}},
    {0xfc,{{0xd3}, {0xb2}}},
    {0xfc,{{0xd4}, {0xb2}}},
    {0xfc,{{0xd5}, {0xb3}}},
    {0xfc,{{0xd6}, {0xb3}}},
    {0xfc,{{0xd7}, {0xb3}}},
    {0xfc,{{0xd8}, {0xb3}}},
    {0xfd,{{0xd9}, {0xb3}}},
    {0xfd,{{0xda}, {0xb4}}},
    {0xfd,{{0xdb}, {0xb4}}},
    {0xfd,{{0xdc}, {0xb4}}},
    {0xfd,{{0xdd}, {0xb4}}},
    {0xfd,{{0xde}, {0xb4}}},
    {0xfd,{{0xdf}, {0xb5}}},
    {0xfd,{{0xe0}, {0xb5}}},
    {0xfd,{{0xe1}, {0xb5}}},
    {0xfe,{{0xe3}, {0xb5}}},
    {0xfe,{{0xe4}, {0xb5}}},
    {0xfe,{{0xe5}, {0xb5}}},
    {0xfe,{{0xe6}, {0xb5}}},
    {0xfe,{{0xe7}, {0xb6}}},
    {0xfe,{{0xe8}, {0xb6}}},
    {0xfe,{{0xe9}, {0xb6}}},
    {0xfe,{{0xea}, {0xb6}}},
    {0xfe,{{0xeb}, {0xb6}}},
    {0xfe,{{0xec}, {0xb6}}},
    {0xfe,{{0xed}, {0xb6}}},
    {0xfe,{{0xee}, {0xb6}}},
    {0xfe,{{0xef}, {0xb6}}},
    {0xfe,{{0xf0}, {0xb7}}},
    {0xfe,{{0xf1}, {0xb7}}},
    {0xfe,{{0xf2}, {0xb7}}},
    {0xff,{{0xf3}, {0xb7}}},
    {0xff,{{0xf4}, {0xb7}}},
    {0xff,{{0xf5}, {0xb7}}},
    {0xff,{{0xf6}, {0xb7}}},
    {0xff,{{0xf7}, {0xb7}}},
    {0xff,{{0xf7}, {0xb7}}},
    {0xff,{{0xf7}, {0xb7}}},
    {0xff,{{0xf7}, {0xb7}}},
    {0xff,{{0xf7}, {0xb7}}},
    {0xff,{{0xf7}, {0xb7}}},
    {0xff,{{0xf7}, {0xb8}}},
    {0xff,{{0xf7}, {0xb8}}},
    {0xff,{{0xf7}, {0xb8}}},
    {0xff,{{0xf7}, {0xb8}}},
    {0xff,{{0xf7}, {0xb8}}},
    {0xff,{{0xf7}, {0xb8}}},
},{
    {0x0,{{0x47}, {0x8}}},
    {0x0,{{0x47}, {0x8}}},
    {0x0,{{0x47}, {0x8}}},
    {0x0,{{0x47}, {0x8}}},
    {0x0,{{0x47}, {0x8}}},
    {0x0,{{0x47}, {0x8}}},
    {0x0,{{0x48}, {0x8}}},
    {0x0,{{0x48}, {0x8}}},
    {0x0,{{0x48}, {0x8}}},
    {0x0,{{0x48}, {0x8}}},
    {0x0,{{0x48}, {0x8}}},
    {0x0,{{0x48}, {0x8}}},
    {0x0,{{0x48}, {0x9}}},
    {0x0,{{0x48}, {0xa}}},
    {0x0,{{0x48}, {0xb}}},
    {0x0,{{0x48}, {0xc}}},
    {0x1,{{0x48}, {0xd}}},
    {0x1,{{0x48}, {0xe}}},
    {0x1,{{0x48}, {0xf}}},
    {0x1,{{0x49}, {0x10}}},
    {0x1,{{0x49}, {0x11}}},
    {0x1,{{0x49}, {0x12}}},
    {0x1,{{0x49}, {0x13}}},
    {0x1,{{0x49}, {0x14}}},
    {0x1,{{0x49}, {0x15}}},
    {0x1,{{0x49}, {0x16}}},
    {0x1,{{0x49}, {0x17}}},
    {0x1,{{0x49}, {0x18}}},
    {0x1,{{0x4a}, {0x19}}},
    {0x1,{{0x4a}, {0x1a}}},
    {0x1,{{0x4a}, {0x1b}}},
    {0x1,{{0x4a}, {0x1c}}},
    {0x2,{{0x4a}, {0x1e}}},
    {0x2,{{0x4a}, {0x1f}}},
    {0x2,{{0x4a}, {0x20}}},
    {0x2,{{0x4b}, {0x21}}},
    {0x2,{{0x4b}, {0x22}}},
    {0x2,{{0x4b}, {0x23}}},
    {0x2,{{0x4b}, {0x24}}},
    {0x2,{{0x4b}, {0x25}}},
    {0x2,{{0x4c}, {0x26}}},
    {0x3,{{0x4c}, {0x27}}},
    {0x3,{{0x4c}, {0x28}}},
    {0x3,{{0x4c}, {0x29}}},
    {0x3,{{0x4c}, {0x2a}}},
    {0x3,{{0x4d}, {0x2b}}},
    {0x3,{{0x4d}, {0x2c}}},
    {0x3,{{0x4d}, {0x2d}}},
    {0x4,{{0x4d}, {0x2e}}},
    {0x4,{{0x4e}, {0x2f}}},
    {0x4,{{0x4e}, {0x30}}},
    {0x4,{{0x4e}, {0x31}}},
    {0x4,{{0x4f}, {0x32}}},
    {0x5,{{0x4f}, {0x33}}},
    {0x5,{{0x4f}, {0x34}}},
    {0x5,{{0x4f}, {0x35}}},
    {0x5,{{0x50}, {0x36}}},
    {0x6,{{0x50}, {0x37}}},
    {0x6,{{0x51}, {0x38}}},
    {0x6,{{0x51}, {0x39}}},
    {0x6,{{0x51}, {0x3a}}},
    {0x7,{{0x52}, {0x3b}}},
    {0x7,{{0x52}, {0x3c}}},
    {0x7,{{0x52}, {0x3d}}},
    {0x8,{{0x53}, {0x3e}}},
    {0x8,{{0x53}, {0x3f}}},
    {0x9,{{0x54}, {0x40}}},
    {0x9,{{0x54}, {0x41}}},
    {0x9,{{0x55}, {0x42}}},
    {0xa,{{0x55}, {0x43}}},
    {0xa,{{0x56}, {0x44}}},
    {0xb,{{0x56}, {0x45}}},
    {0xb,{{0x57}, {0x46}}},
    {0xc,{{0x57}, {0x47}}},
    {0xc,{{0x58}, {0x48}}},
    {0xd,{{0x58}, {0x49}}},
    {0xd,{{0x59}, {0x4a}}},
    {0xe,{{0x59}, {0x4b}}},
    {0xf,{{0x5a}, {0x4c}}},
    {0xf,{{0x5a}, {0x4d}}},
    {0x10,{{0x5b}, {0x4e}}},
    {0x11,{{0x5c}, {0x4f}}},
    {0x12,{{0x5c}, {0x50}}},
    {0x12,{{0x5d}, {0x51}}},
    {0x13,{{0x5d}, {0x52}}},
    {0x14,{{0x5e}, {0x53}}},
    {0x15,{{0x5f}, {0x54}}},
    {0x16,{{0x5f}, {0x55}}},
    {0x17,{{0x60}, {0x57}}},
    {0x18,{{0x61}, {0x58}}},
    {0x19,{{0x61}, {0x59}}},
    {0x1a,{{0x62}, {0x5a}}},
    {0x1b,{{0x63}, {0x5b}}},
    {0x1d,{{0x64}, {0x5c}}},
    {0x1e,{{0x64}, {0x5d}}},
    {0x1f,{{0x65}, {0x5e}}},
    {0x21,{{0x66}, {0x5f}}},
    {0x22,{{0x67}, {0x60}}},
    {0x24,{{0x67}, {0x61}}},
    {0x25,{{0x68}, {0x62}}},
    {0x27,{{0x69}, {0x63}}},
    {0x29,{{0x6a}, {0x64}}},
    {0x2b,{{0x6a}, {0x65}}},
    {0x2d,{{0x6b}, {0x66}}},
    {0x2f,{{0x6c}, {0x67}}},
    {0x31,{{0x6d}, {0x68}}},
    {0x33,{{0x6e}, {0x69}}},
    {0x35,{{0x6e}, {0x6a}}},
    {0x38,{{0x6f}, {0x6b}}},
    {0x3a,{{0x70}, {0x6c}}},
    {0x3d,{{0x71}, {0x6d}}},
    {0x3f,{{0x72}, {0x6e}}},
    {0x42,{{0x73}, {0x6f}}},
    {0x45,{{0x73}, {0x70}}},
    {0x48,{{0x74}, {0x71}}},
    {0x4c,{{0x75}, {0x72}}},
    {0x4f,{{0x76}, {0x73}}},
    {0x52,{{0x77}, {0x74}}},
    {0x56,{{0x78}, {0x75}}},
    {0x5a,{{0x79}, {0x76}}},
    {0x5e,{{0x7a}, {0x77}}},
    {0x62,{{0x7b}, {0x78}}},
    {0x67,{{0x7b}, {0x79}}},
    {0x6b,{{0x7c}, {0x7a}}},
    {0x70,{{0x7d}, {0x7b}}},
    {0x75,{{0x7e}, {0x7c}}},
    {0x7a,{{0x7f}, {0x7d}}},
    {0x80,{{0x80}, {0x7e}}},
    {0x7f,{{0x81}, {0x7f}}},
    {0x85,{{0x82}, {0x80}}},
    {0x8a,{{0x83}, {0x81}}},
    {0x8f,{{0x84}, {0x82}}},
    {0x94,{{0x85}, {0x83}}},
    {0x98,{{0x86}, {0x84}}},
    {0x9d,{{0x87}, {0x84}}},
    {0xa1,{{0x88}, {0x85}}},
    {0xa5,{{0x89}, {0x86}}},
    {0xa9,{{0x8a}, {0x87}}},
    {0xad,{{0x8b}, {0x88}}},
    {0xb0,{{0x8c}, {0x89}}},
    {0xb3,{{0x8d}, {0x8a}}},
    {0xb7,{{0x8e}, {0x8b}}},
    {0xba,{{0x8f}, {0x8c}}},
    {0xbd,{{0x90}, {0x8c}}},
    {0xc0,{{0x91}, {0x8d}}},
    {0xc2,{{0x92}, {0x8e}}},
    {0xc5,{{0x93}, {0x8f}}},
    {0xc7,{{0x94}, {0x90}}},
    {0xca,{{0x95}, {0x91}}},
    {0xcc,{{0x96}, {0x91}}},
    {0xce,{{0x97}, {0x92}}},
    {0xd0,{{0x98}, {0x93}}},
    {0xd2,{{0x99}, {0x94}}},
    {0xd4,{{0x9a}, {0x95}}},
    {0xd6,{{0x9b}, {0x95}}},
    {0xd8,{{0x9c}, {0x96}}},
    {0xda,{{0x9d}, {0x97}}},
    {0xdb,{{0x9e}, {0x98}}},
    {0xdd,{{0x9f}, {0x98}}},
    {0xde,{{0xa0}, {0x99}}},
    {0xe0,{{0xa1}, {0x9a}}},
    {0xe1,{{0xa2}, {0x9b}}},
    {0xe2,{{0xa3}, {0x9b}}},
    {0xe4,{{0xa4}, {0x9c}}},
    {0xe5,{{0xa5}, {0x9d}}},
    {0xe6,{{0xa6}, {0x9e}}},
    {0xe7,{{0xa7}, {0x9e}}},
    {0xe8,{{0xa8}, {0x9f}}},
    {0xe9,{{0xaa}, {0xa0}}},
    {0xea,{{0xab}, {0xa0}}},
    {0xeb,{{0xac}, {0xa1}}},
    {0xec,{{0xad}, {0xa2}}},
    {0xed,{{0xae}, {0xa2}}},
    {0xed,{{0xaf}, {0xa3}}},
    {0xee,{{0xb0}, {0xa3}}},
    {0xef,{{0xb1}, {0xa4}}},
    {0xf0,{{0xb2}, {0xa5}}},
    {0xf0,{{0xb3}, {0xa5}}},
    {0xf1,{{0xb4}, {0xa6}}},
    {0xf2,{{0xb5}, {0xa6}}},
    {0xf2,{{0xb6}, {0xa7}}},
    {0xf3,{{0xb7}, {0xa7}}},
    {0xf3,{{0xb8}, {0xa8}}},
    {0xf4,{{0xb9}, {0xa8}}},
    {0xf4,{{0xba}, {0xa9}}},
    {0xf5,{{0xbb}, {0xa9}}},
    {0xf5,{{0xbc}, {0xaa}}},
    {0xf6,{{0xbd}, {0xaa}}},
    {0xf6,{{0xbe}, {0xab}}},
    {0xf6,{{0xbf}, {0xab}}},
    {0xf7,{{0xc0}, {0xac}}},
    {0xf7,{{0xc1}, {0xac}}},
    {0xf8,{{0xc2}, {0xad}}},
    {0xf8,{{0xc3}, {0xad}}},
    {0xf8,{{0xc4}, {0xad}}},
    {0xf9,{{0xc5}, {0xae}}},
    {0xf9,{{0xc6}, {0xae}}},
    {0xf9,{{0xc7}, {0xae}}},
    {0xf9,{{0xc8}, {0xaf}}},
    {0xfa,{{0xc9}, {0xaf}}},
    {0xfa,{{0xca}, {0xb0}}},
    {0xfa,{{0xcb}, {0xb0}}},
    {0xfa,{{0xcc}, {0xb0}}},
    {0xfb,{{0xcd}, {0xb0}}},
    {0xfb,{{0xce}, {0xb1}}},
    {0xfb,{{0xcf}, {0xb1}}},
    {0xfb,{{0xd0}, {0xb1}}},
    {0xfb,{{0xd1}, {0xb2}}},
    {0xfc,{{0xd2}, {0xb2}}},
    {0xfc,{{0xd3}, {0xb2}}},
    {0xfc,{{0xd4}, {0xb2}}},
    {0xfc,{{0xd5}, {0xb3}}},
    {0xfc,{{0xd6}, {0xb3}}},
    {0xfc,{{0xd7}, {0xb3}}},
    {0xfc,{{0xd8}, {0xb3}}},
    {0xfd,{{0xd9}, {0xb3}}},
    {0xfd,{{0xda}, {0xb4}}},
    {0xfd,{{0xdb}, {0xb4}}},
    {0xfd,{{0xdc}, {0xb4}}},
    {0xfd,{{0xdd}, {0xb4}}},
    {0xfd,{{0xde}, {0xb4}}},
    {0xfd,{{0xdf}, {0xb5}}},
    {0xfd,{{0xe0}, {0xb5}}},
    {0xfd,{{0xe1}, {0xb5}}},
    {0xfe,{{0xe3}, {0xb5}}},
    {0xfe,{{0xe4}, {0xb5}}},
    {0xfe,{{0xe5}, {0xb5}}},
    {0xfe,{{0xe6}, {0xb5}}},
    {0xfe,{{0xe7}, {0xb6}}},
    {0xfe,{{0xe8}, {0xb6}}},
    {0xfe,{{0xe9}, {0xb6}}},
    {0xfe,{{0xea}, {0xb6}}},
    {0xfe,{{0xeb}, {0xb6}}},
    {0xfe,{{0xec}, {0xb6}}},
    {0xfe,{{0xed}, {0xb6}}},
    {0xfe,{{0xee}, {0xb6}}},
    {0xfe,{{0xef}, {0xb6}}},
    {0xfe,{{0xf0}, {0xb7}}},
    {0xfe,{{0xf1}, {0xb7}}},
    {0xfe,{{0xf2}, {0xb7}}},
    {0xff,{{0xf3}, {0xb7}}},
    {0xff,{{0xf4}, {0xb7}}},
    {0xff,{{0xf5}, {0xb7}}},
    {0xff,{{0xf6}, {0xb7}}},
    {0xff,{{0xf7}, {0xb7}}},
    {0xff,{{0xf7}, {0xb7}}},
    {0xff,{{0xf7}, {0xb7}}},
    {0xff,{{0xf7}, {0xb7}}},
    {0xff,{{0xf7}, {0xb7}}},
    {0xff,{{0xf7}, {0xb7}}},
    {0xff,{{0xf7}, {0xb8}}},
    {0xff,{{0xf7}, {0xb8}}},
    {0xff,{{0xf7}, {0xb8}}},
    {0xff,{{0xf7}, {0xb8}}},
    {0xff,{{0xf7}, {0xb8}}},
    {0xff,{{0xf7}, {0xb8}}},
},{
    {0x0,{{0x47}, {0x8}}},
    {0x0,{{0x47}, {0x8}}},
    {0x0,{{0x47}, {0x8}}},
    {0x0,{{0x47}, {0x8}}},
    {0x0,{{0x47}, {0x8}}},
    {0x0,{{0x47}, {0x8}}},
    {0x0,{{0x48}, {0x8}}},
    {0x0,{{0x48}, {0x8}}},
    {0x0,{{0x48}, {0x8}}},
    {0x0,{{0x48}, {0x8}}},
    {0x0,{{0x48}, {0x8}}},
    {0x0,{{0x48}, {0x8}}},
    {0x0,{{0x48}, {0x9}}},
    {0x0,{{0x48}, {0xa}}},
    {0x0,{{0x48}, {0xb}}},
    {0x0,{{0x48}, {0xc}}},
    {0x1,{{0x48}, {0xd}}},
    {0x1,{{0x48}, {0xe}}},
    {0x1,{{0x48}, {0xf}}},
    {0x1,{{0x49}, {0x10}}},
    {0x1,{{0x49}, {0x11}}},
    {0x1,{{0x49}, {0x12}}},
    {0x1,{{0x49}, {0x13}}},
    {0x1,{{0x49}, {0x14}}},
    {0x1,{{0x49}, {0x15}}},
    {0x1,{{0x49}, {0x16}}},
    {0x1,{{0x49}, {0x17}}},
    {0x1,{{0x49}, {0x18}}},
    {0x1,{{0x4a}, {0x19}}},
    {0x1,{{0x4a}, {0x1a}}},
    {0x1,{{0x4a}, {0x1b}}},
    {0x1,{{0x4a}, {0x1c}}},
    {0x2,{{0x4a}, {0x1e}}},
    {0x2,{{0x4a}, {0x1f}}},
    {0x2,{{0x4a}, {0x20}}},
    {0x2,{{0x4b}, {0x21}}},
    {0x2,{{0x4b}, {0x22}}},
    {0x2,{{0x4b}, {0x23}}},
    {0x2,{{0x4b}, {0x24}}},
    {0x2,{{0x4b}, {0x25}}},
    {0x2,{{0x4c}, {0x26}}},
    {0x3,{{0x4c}, {0x27}}},
    {0x3,{{0x4c}, {0x28}}},
    {0x3,{{0x4c}, {0x29}}},
    {0x3,{{0x4c}, {0x2a}}},
    {0x3,{{0x4d}, {0x2b}}},
    {0x3,{{0x4d}, {0x2c}}},
    {0x3,{{0x4d}, {0x2d}}},
    {0x4,{{0x4d}, {0x2e}}},
    {0x4,{{0x4e}, {0x2f}}},
    {0x4,{{0x4e}, {0x30}}},
    {0x4,{{0x4e}, {0x31}}},
    {0x4,{{0x4f}, {0x32}}},
    {0x5,{{0x4f}, {0x33}}},
    {0x5,{{0x4f}, {0x34}}},
    {0x5,{{0x4f}, {0x35}}},
    {0x5,{{0x50}, {0x36}}},
    {0x6,{{0x50}, {0x37}}},
    {0x6,{{0x51}, {0x38}}},
    {0x6,{{0x51}, {0x39}}},
    {0x6,{{0x51}, {0x3a}}},
    {0x7,{{0x52}, {0x3b}}},
    {0x7,{{0x52}, {0x3c}}},
    {0x7,{{0x52}, {0x3d}}},
    {0x8,{{0x53}, {0x3e}}},
    {0x8,{{0x53}, {0x3f}}},
    {0x9,{{0x54}, {0x40}}},
    {0x9,{{0x54}, {0x41}}},
    {0x9,{{0x55}, {0x42}}},
    {0xa,{{0x55}, {0x43}}},
    {0xa,{{0x56}, {0x44}}},
    {0xb,{{0x56}, {0x45}}},
    {0xb,{{0x57}, {0x46}}},
    {0xc,{{0x57}, {0x47}}},
    {0xc,{{0x58}, {0x48}}},
    {0xd,{{0x58}, {0x49}}},
    {0xd,{{0x59}, {0x4a}}},
    {0xe,{{0x59}, {0x4b}}},
    {0xf,{{0x5a}, {0x4c}}},
    {0xf,{{0x5a}, {0x4d}}},
    {0x10,{{0x5b}, {0x4e}}},
    {0x11,{{0x5c}, {0x4f}}},
    {0x12,{{0x5c}, {0x50}}},
    {0x12,{{0x5d}, {0x51}}},
    {0x13,{{0x5d}, {0x52}}},
    {0x14,{{0x5e}, {0x53}}},
    {0x15,{{0x5f}, {0x54}}},
    {0x16,{{0x5f}, {0x55}}},
    {0x17,{{0x60}, {0x57}}},
    {0x18,{{0x61}, {0x58}}},
    {0x19,{{0x61}, {0x59}}},
    {0x1a,{{0x62}, {0x5a}}},
    {0x1b,{{0x63}, {0x5b}}},
    {0x1d,{{0x64}, {0x5c}}},
    {0x1e,{{0x64}, {0x5d}}},
    {0x1f,{{0x65}, {0x5e}}},
    {0x21,{{0x66}, {0x5f}}},
    {0x22,{{0x67}, {0x60}}},
    {0x24,{{0x67}, {0x61}}},
    {0x25,{{0x68}, {0x62}}},
    {0x27,{{0x69}, {0x63}}},
    {0x29,{{0x6a}, {0x64}}},
    {0x2b,{{0x6a}, {0x65}}},
    {0x2d,{{0x6b}, {0x66}}},
    {0x2f,{{0x6c}, {0x67}}},
    {0x31,{{0x6d}, {0x68}}},
    {0x33,{{0x6e}, {0x69}}},
    {0x35,{{0x6e}, {0x6a}}},
    {0x38,{{0x6f}, {0x6b}}},
    {0x3a,{{0x70}, {0x6c}}},
    {0x3d,{{0x71}, {0x6d}}},
    {0x3f,{{0x72}, {0x6e}}},
    {0x42,{{0x73}, {0x6f}}},
    {0x45,{{0x73}, {0x70}}},
    {0x48,{{0x74}, {0x71}}},
    {0x4c,{{0x75}, {0x72}}},
    {0x4f,{{0x76}, {0x73}}},
    {0x52,{{0x77}, {0x74}}},
    {0x56,{{0x78}, {0x75}}},
    {0x5a,{{0x79}, {0x76}}},
    {0x5e,{{0x7a}, {0x77}}},
    {0x62,{{0x7b}, {0x78}}},
    {0x67,{{0x7b}, {0x79}}},
    {0x6b,{{0x7c}, {0x7a}}},
    {0x70,{{0x7d}, {0x7b}}},
    {0x75,{{0x7e}, {0x7c}}},
    {0x7a,{{0x7f}, {0x7d}}},
    {0x80,{{0x80}, {0x7e}}},
    {0x7f,{{0x81}, {0x7f}}},
    {0x85,{{0x82}, {0x80}}},
    {0x8a,{{0x83}, {0x81}}},
    {0x8f,{{0x84}, {0x82}}},
    {0x94,{{0x85}, {0x83}}},
    {0x98,{{0x86}, {0x84}}},
    {0x9d,{{0x87}, {0x84}}},
    {0xa1,{{0x88}, {0x85}}},
    {0xa5,{{0x89}, {0x86}}},
    {0xa9,{{0x8a}, {0x87}}},
    {0xad,{{0x8b}, {0x88}}},
    {0xb0,{{0x8c}, {0x89}}},
    {0xb3,{{0x8d}, {0x8a}}},
    {0xb7,{{0x8e}, {0x8b}}},
    {0xba,{{0x8f}, {0x8c}}},
    {0xbd,{{0x90}, {0x8c}}},
    {0xc0,{{0x91}, {0x8d}}},
    {0xc2,{{0x92}, {0x8e}}},
    {0xc5,{{0x93}, {0x8f}}},
    {0xc7,{{0x94}, {0x90}}},
    {0xca,{{0x95}, {0x91}}},
    {0xcc,{{0x96}, {0x91}}},
    {0xce,{{0x97}, {0x92}}},
    {0xd0,{{0x98}, {0x93}}},
    {0xd2,{{0x99}, {0x94}}},
    {0xd4,{{0x9a}, {0x95}}},
    {0xd6,{{0x9b}, {0x95}}},
    {0xd8,{{0x9c}, {0x96}}},
    {0xda,{{0x9d}, {0x97}}},
    {0xdb,{{0x9e}, {0x98}}},
    {0xdd,{{0x9f}, {0x98}}},
    {0xde,{{0xa0}, {0x99}}},
    {0xe0,{{0xa1}, {0x9a}}},
    {0xe1,{{0xa2}, {0x9b}}},
    {0xe2,{{0xa3}, {0x9b}}},
    {0xe4,{{0xa4}, {0x9c}}},
    {0xe5,{{0xa5}, {0x9d}}},
    {0xe6,{{0xa6}, {0x9e}}},
    {0xe7,{{0xa7}, {0x9e}}},
    {0xe8,{{0xa8}, {0x9f}}},
    {0xe9,{{0xaa}, {0xa0}}},
    {0xea,{{0xab}, {0xa0}}},
    {0xeb,{{0xac}, {0xa1}}},
    {0xec,{{0xad}, {0xa2}}},
    {0xed,{{0xae}, {0xa2}}},
    {0xed,{{0xaf}, {0xa3}}},
    {0xee,{{0xb0}, {0xa3}}},
    {0xef,{{0xb1}, {0xa4}}},
    {0xf0,{{0xb2}, {0xa5}}},
    {0xf0,{{0xb3}, {0xa5}}},
    {0xf1,{{0xb4}, {0xa6}}},
    {0xf2,{{0xb5}, {0xa6}}},
    {0xf2,{{0xb6}, {0xa7}}},
    {0xf3,{{0xb7}, {0xa7}}},
    {0xf3,{{0xb8}, {0xa8}}},
    {0xf4,{{0xb9}, {0xa8}}},
    {0xf4,{{0xba}, {0xa9}}},
    {0xf5,{{0xbb}, {0xa9}}},
    {0xf5,{{0xbc}, {0xaa}}},
    {0xf6,{{0xbd}, {0xaa}}},
    {0xf6,{{0xbe}, {0xab}}},
    {0xf6,{{0xbf}, {0xab}}},
    {0xf7,{{0xc0}, {0xac}}},
    {0xf7,{{0xc1}, {0xac}}},
    {0xf8,{{0xc2}, {0xad}}},
    {0xf8,{{0xc3}, {0xad}}},
    {0xf8,{{0xc4}, {0xad}}},
    {0xf9,{{0xc5}, {0xae}}},
    {0xf9,{{0xc6}, {0xae}}},
    {0xf9,{{0xc7}, {0xae}}},
    {0xf9,{{0xc8}, {0xaf}}},
    {0xfa,{{0xc9}, {0xaf}}},
    {0xfa,{{0xca}, {0xb0}}},
    {0xfa,{{0xcb}, {0xb0}}},
    {0xfa,{{0xcc}, {0xb0}}},
    {0xfb,{{0xcd}, {0xb0}}},
    {0xfb,{{0xce}, {0xb1}}},
    {0xfb,{{0xcf}, {0xb1}}},
    {0xfb,{{0xd0}, {0xb1}}},
    {0xfb,{{0xd1}, {0xb2}}},
    {0xfc,{{0xd2}, {0xb2}}},
    {0xfc,{{0xd3}, {0xb2}}},
    {0xfc,{{0xd4}, {0xb2}}},
    {0xfc,{{0xd5}, {0xb3}}},
    {0xfc,{{0xd6}, {0xb3}}},
    {0xfc,{{0xd7}, {0xb3}}},
    {0xfc,{{0xd8}, {0xb3}}},
    {0xfd,{{0xd9}, {0xb3}}},
    {0xfd,{{0xda}, {0xb4}}},
    {0xfd,{{0xdb}, {0xb4}}},
    {0xfd,{{0xdc}, {0xb4}}},
    {0xfd,{{0xdd}, {0xb4}}},
    {0xfd,{{0xde}, {0xb4}}},
    {0xfd,{{0xdf}, {0xb5}}},
    {0xfd,{{0xe0}, {0xb5}}},
    {0xfd,{{0xe1}, {0xb5}}},
    {0xfe,{{0xe3}, {0xb5}}},
    {0xfe,{{0xe4}, {0xb5}}},
    {0xfe,{{0xe5}, {0xb5}}},
    {0xfe,{{0xe6}, {0xb5}}},
    {0xfe,{{0xe7}, {0xb6}}},
    {0xfe,{{0xe8}, {0xb6}}},
    {0xfe,{{0xe9}, {0xb6}}},
    {0xfe,{{0xea}, {0xb6}}},
    {0xfe,{{0xeb}, {0xb6}}},
    {0xfe,{{0xec}, {0xb6}}},
    {0xfe,{{0xed}, {0xb6}}},
    {0xfe,{{0xee}, {0xb6}}},
    {0xfe,{{0xef}, {0xb6}}},
    {0xfe,{{0xf0}, {0xb7}}},
    {0xfe,{{0xf1}, {0xb7}}},
    {0xfe,{{0xf2}, {0xb7}}},
    {0xff,{{0xf3}, {0xb7}}},
    {0xff,{{0xf4}, {0xb7}}},
    {0xff,{{0xf5}, {0xb7}}},
    {0xff,{{0xf6}, {0xb7}}},
    {0xff,{{0xf7}, {0xb7}}},
    {0xff,{{0xf7}, {0xb7}}},
    {0xff,{{0xf7}, {0xb7}}},
    {0xff,{{0xf7}, {0xb7}}},
    {0xff,{{0xf7}, {0xb7}}},
    {0xff,{{0xf7}, {0xb7}}},
    {0xff,{{0xf7}, {0xb8}}},
    {0xff,{{0xf7}, {0xb8}}},
    {0xff,{{0xf7}, {0xb8}}},
    {0xff,{{0xf7}, {0xb8}}},
    {0xff,{{0xf7}, {0xb8}}},
    {0xff,{{0xf7}, {0xb8}}}},
    {
    {0x0,{{0x47}, {0x8}}},
    {0x0,{{0x47}, {0x8}}},
    {0x0,{{0x47}, {0x8}}},
    {0x0,{{0x47}, {0x8}}},
    {0x0,{{0x47}, {0x8}}},
    {0x0,{{0x47}, {0x8}}},
    {0x0,{{0x48}, {0x8}}},
    {0x0,{{0x48}, {0x8}}},
    {0x0,{{0x48}, {0x8}}},
    {0x0,{{0x48}, {0x8}}},
    {0x0,{{0x48}, {0x8}}},
    {0x0,{{0x48}, {0x8}}},
    {0x0,{{0x48}, {0x9}}},
    {0x0,{{0x48}, {0xa}}},
    {0x0,{{0x48}, {0xb}}},
    {0x0,{{0x48}, {0xc}}},
    {0x1,{{0x48}, {0xd}}},
    {0x1,{{0x48}, {0xe}}},
    {0x1,{{0x48}, {0xf}}},
    {0x1,{{0x49}, {0x10}}},
    {0x1,{{0x49}, {0x11}}},
    {0x1,{{0x49}, {0x12}}},
    {0x1,{{0x49}, {0x13}}},
    {0x1,{{0x49}, {0x14}}},
    {0x1,{{0x49}, {0x15}}},
    {0x1,{{0x49}, {0x16}}},
    {0x1,{{0x49}, {0x17}}},
    {0x1,{{0x49}, {0x18}}},
    {0x1,{{0x4a}, {0x19}}},
    {0x1,{{0x4a}, {0x1a}}},
    {0x1,{{0x4a}, {0x1b}}},
    {0x1,{{0x4a}, {0x1c}}},
    {0x2,{{0x4a}, {0x1e}}},
    {0x2,{{0x4a}, {0x1f}}},
    {0x2,{{0x4a}, {0x20}}},
    {0x2,{{0x4b}, {0x21}}},
    {0x2,{{0x4b}, {0x22}}},
    {0x2,{{0x4b}, {0x23}}},
    {0x2,{{0x4b}, {0x24}}},
    {0x2,{{0x4b}, {0x25}}},
    {0x2,{{0x4c}, {0x26}}},
    {0x3,{{0x4c}, {0x27}}},
    {0x3,{{0x4c}, {0x28}}},
    {0x3,{{0x4c}, {0x29}}},
    {0x3,{{0x4c}, {0x2a}}},
    {0x3,{{0x4d}, {0x2b}}},
    {0x3,{{0x4d}, {0x2c}}},
    {0x3,{{0x4d}, {0x2d}}},
    {0x4,{{0x4d}, {0x2e}}},
    {0x4,{{0x4e}, {0x2f}}},
    {0x4,{{0x4e}, {0x30}}},
    {0x4,{{0x4e}, {0x31}}},
    {0x4,{{0x4f}, {0x32}}},
    {0x5,{{0x4f}, {0x33}}},
    {0x5,{{0x4f}, {0x34}}},
    {0x5,{{0x4f}, {0x35}}},
    {0x5,{{0x50}, {0x36}}},
    {0x6,{{0x50}, {0x37}}},
    {0x6,{{0x51}, {0x38}}},
    {0x6,{{0x51}, {0x39}}},
    {0x6,{{0x51}, {0x3a}}},
    {0x7,{{0x52}, {0x3b}}},
    {0x7,{{0x52}, {0x3c}}},
    {0x7,{{0x52}, {0x3d}}},
    {0x8,{{0x53}, {0x3e}}},
    {0x8,{{0x53}, {0x3f}}},
    {0x9,{{0x54}, {0x40}}},
    {0x9,{{0x54}, {0x41}}},
    {0x9,{{0x55}, {0x42}}},
    {0xa,{{0x55}, {0x43}}},
    {0xa,{{0x56}, {0x44}}},
    {0xb,{{0x56}, {0x45}}},
    {0xb,{{0x57}, {0x46}}},
    {0xc,{{0x57}, {0x47}}},
    {0xc,{{0x58}, {0x48}}},
    {0xd,{{0x58}, {0x49}}},
    {0xd,{{0x59}, {0x4a}}},
    {0xe,{{0x59}, {0x4b}}},
    {0xf,{{0x5a}, {0x4c}}},
    {0xf,{{0x5a}, {0x4d}}},
    {0x10,{{0x5b}, {0x4e}}},
    {0x11,{{0x5c}, {0x4f}}},
    {0x12,{{0x5c}, {0x50}}},
    {0x12,{{0x5d}, {0x51}}},
    {0x13,{{0x5d}, {0x52}}},
    {0x14,{{0x5e}, {0x53}}},
    {0x15,{{0x5f}, {0x54}}},
    {0x16,{{0x5f}, {0x55}}},
    {0x17,{{0x60}, {0x57}}},
    {0x18,{{0x61}, {0x58}}},
    {0x19,{{0x61}, {0x59}}},
    {0x1a,{{0x62}, {0x5a}}},
    {0x1b,{{0x63}, {0x5b}}},
    {0x1d,{{0x64}, {0x5c}}},
    {0x1e,{{0x64}, {0x5d}}},
    {0x1f,{{0x65}, {0x5e}}},
    {0x21,{{0x66}, {0x5f}}},
    {0x22,{{0x67}, {0x60}}},
    {0x24,{{0x67}, {0x61}}},
    {0x25,{{0x68}, {0x62}}},
    {0x27,{{0x69}, {0x63}}},
    {0x29,{{0x6a}, {0x64}}},
    {0x2b,{{0x6a}, {0x65}}},
    {0x2d,{{0x6b}, {0x66}}},
    {0x2f,{{0x6c}, {0x67}}},
    {0x31,{{0x6d}, {0x68}}},
    {0x33,{{0x6e}, {0x69}}},
    {0x35,{{0x6e}, {0x6a}}},
    {0x38,{{0x6f}, {0x6b}}},
    {0x3a,{{0x70}, {0x6c}}},
    {0x3d,{{0x71}, {0x6d}}},
    {0x3f,{{0x72}, {0x6e}}},
    {0x42,{{0x73}, {0x6f}}},
    {0x45,{{0x73}, {0x70}}},
    {0x48,{{0x74}, {0x71}}},
    {0x4c,{{0x75}, {0x72}}},
    {0x4f,{{0x76}, {0x73}}},
    {0x52,{{0x77}, {0x74}}},
    {0x56,{{0x78}, {0x75}}},
    {0x5a,{{0x79}, {0x76}}},
    {0x5e,{{0x7a}, {0x77}}},
    {0x62,{{0x7b}, {0x78}}},
    {0x67,{{0x7b}, {0x79}}},
    {0x6b,{{0x7c}, {0x7a}}},
    {0x70,{{0x7d}, {0x7b}}},
    {0x75,{{0x7e}, {0x7c}}},
    {0x7a,{{0x7f}, {0x7d}}},
    {0x80,{{0x80}, {0x7e}}},
    {0x7f,{{0x81}, {0x7f}}},
    {0x85,{{0x82}, {0x80}}},
    {0x8a,{{0x83}, {0x81}}},
    {0x8f,{{0x84}, {0x82}}},
    {0x94,{{0x85}, {0x83}}},
    {0x98,{{0x86}, {0x84}}},
    {0x9d,{{0x87}, {0x84}}},
    {0xa1,{{0x88}, {0x85}}},
    {0xa5,{{0x89}, {0x86}}},
    {0xa9,{{0x8a}, {0x87}}},
    {0xad,{{0x8b}, {0x88}}},
    {0xb0,{{0x8c}, {0x89}}},
    {0xb3,{{0x8d}, {0x8a}}},
    {0xb7,{{0x8e}, {0x8b}}},
    {0xba,{{0x8f}, {0x8c}}},
    {0xbd,{{0x90}, {0x8c}}},
    {0xc0,{{0x91}, {0x8d}}},
    {0xc2,{{0x92}, {0x8e}}},
    {0xc5,{{0x93}, {0x8f}}},
    {0xc7,{{0x94}, {0x90}}},
    {0xca,{{0x95}, {0x91}}},
    {0xcc,{{0x96}, {0x91}}},
    {0xce,{{0x97}, {0x92}}},
    {0xd0,{{0x98}, {0x93}}},
    {0xd2,{{0x99}, {0x94}}},
    {0xd4,{{0x9a}, {0x95}}},
    {0xd6,{{0x9b}, {0x95}}},
    {0xd8,{{0x9c}, {0x96}}},
    {0xda,{{0x9d}, {0x97}}},
    {0xdb,{{0x9e}, {0x98}}},
    {0xdd,{{0x9f}, {0x98}}},
    {0xde,{{0xa0}, {0x99}}},
    {0xe0,{{0xa1}, {0x9a}}},
    {0xe1,{{0xa2}, {0x9b}}},
    {0xe2,{{0xa3}, {0x9b}}},
    {0xe4,{{0xa4}, {0x9c}}},
    {0xe5,{{0xa5}, {0x9d}}},
    {0xe6,{{0xa6}, {0x9e}}},
    {0xe7,{{0xa7}, {0x9e}}},
    {0xe8,{{0xa8}, {0x9f}}},
    {0xe9,{{0xaa}, {0xa0}}},
    {0xea,{{0xab}, {0xa0}}},
    {0xeb,{{0xac}, {0xa1}}},
    {0xec,{{0xad}, {0xa2}}},
    {0xed,{{0xae}, {0xa2}}},
    {0xed,{{0xaf}, {0xa3}}},
    {0xee,{{0xb0}, {0xa3}}},
    {0xef,{{0xb1}, {0xa4}}},
    {0xf0,{{0xb2}, {0xa5}}},
    {0xf0,{{0xb3}, {0xa5}}},
    {0xf1,{{0xb4}, {0xa6}}},
    {0xf2,{{0xb5}, {0xa6}}},
    {0xf2,{{0xb6}, {0xa7}}},
    {0xf3,{{0xb7}, {0xa7}}},
    {0xf3,{{0xb8}, {0xa8}}},
    {0xf4,{{0xb9}, {0xa8}}},
    {0xf4,{{0xba}, {0xa9}}},
    {0xf5,{{0xbb}, {0xa9}}},
    {0xf5,{{0xbc}, {0xaa}}},
    {0xf6,{{0xbd}, {0xaa}}},
    {0xf6,{{0xbe}, {0xab}}},
    {0xf6,{{0xbf}, {0xab}}},
    {0xf7,{{0xc0}, {0xac}}},
    {0xf7,{{0xc1}, {0xac}}},
    {0xf8,{{0xc2}, {0xad}}},
    {0xf8,{{0xc3}, {0xad}}},
    {0xf8,{{0xc4}, {0xad}}},
    {0xf9,{{0xc5}, {0xae}}},
    {0xf9,{{0xc6}, {0xae}}},
    {0xf9,{{0xc7}, {0xae}}},
    {0xf9,{{0xc8}, {0xaf}}},
    {0xfa,{{0xc9}, {0xaf}}},
    {0xfa,{{0xca}, {0xb0}}},
    {0xfa,{{0xcb}, {0xb0}}},
    {0xfa,{{0xcc}, {0xb0}}},
    {0xfb,{{0xcd}, {0xb0}}},
    {0xfb,{{0xce}, {0xb1}}},
    {0xfb,{{0xcf}, {0xb1}}},
    {0xfb,{{0xd0}, {0xb1}}},
    {0xfb,{{0xd1}, {0xb2}}},
    {0xfc,{{0xd2}, {0xb2}}},
    {0xfc,{{0xd3}, {0xb2}}},
    {0xfc,{{0xd4}, {0xb2}}},
    {0xfc,{{0xd5}, {0xb3}}},
    {0xfc,{{0xd6}, {0xb3}}},
    {0xfc,{{0xd7}, {0xb3}}},
    {0xfc,{{0xd8}, {0xb3}}},
    {0xfd,{{0xd9}, {0xb3}}},
    {0xfd,{{0xda}, {0xb4}}},
    {0xfd,{{0xdb}, {0xb4}}},
    {0xfd,{{0xdc}, {0xb4}}},
    {0xfd,{{0xdd}, {0xb4}}},
    {0xfd,{{0xde}, {0xb4}}},
    {0xfd,{{0xdf}, {0xb5}}},
    {0xfd,{{0xe0}, {0xb5}}},
    {0xfd,{{0xe1}, {0xb5}}},
    {0xfe,{{0xe3}, {0xb5}}},
    {0xfe,{{0xe4}, {0xb5}}},
    {0xfe,{{0xe5}, {0xb5}}},
    {0xfe,{{0xe6}, {0xb5}}},
    {0xfe,{{0xe7}, {0xb6}}},
    {0xfe,{{0xe8}, {0xb6}}},
    {0xfe,{{0xe9}, {0xb6}}},
    {0xfe,{{0xea}, {0xb6}}},
    {0xfe,{{0xeb}, {0xb6}}},
    {0xfe,{{0xec}, {0xb6}}},
    {0xfe,{{0xed}, {0xb6}}},
    {0xfe,{{0xee}, {0xb6}}},
    {0xfe,{{0xef}, {0xb6}}},
    {0xfe,{{0xf0}, {0xb7}}},
    {0xfe,{{0xf1}, {0xb7}}},
    {0xfe,{{0xf2}, {0xb7}}},
    {0xff,{{0xf3}, {0xb7}}},
    {0xff,{{0xf4}, {0xb7}}},
    {0xff,{{0xf5}, {0xb7}}},
    {0xff,{{0xf6}, {0xb7}}},
    {0xff,{{0xf7}, {0xb7}}},
    {0xff,{{0xf7}, {0xb7}}},
    {0xff,{{0xf7}, {0xb7}}},
    {0xff,{{0xf7}, {0xb7}}},
    {0xff,{{0xf7}, {0xb7}}},
    {0xff,{{0xf7}, {0xb7}}},
    {0xff,{{0xf7}, {0xb8}}},
    {0xff,{{0xf7}, {0xb8}}},
    {0xff,{{0xf7}, {0xb8}}},
    {0xff,{{0xf7}, {0xb8}}},
    {0xff,{{0xf7}, {0xb8}}},
    {0xff,{{0xf7}, {0xb8}}}}};
