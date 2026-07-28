#include "chemistry_tables.hpp"

#include <cstdio>

// ---------------------------------------------------------------------------
// The tables (BL-209).
//
// Both arrays are indexed by their enum, so ORDER HERE MUST MATCH ORDER THERE.
// The static_asserts below catch a table that has drifted; tables_self_consistent()
// catches an entry that names a species the dictionary does not have.
// ---------------------------------------------------------------------------

namespace chem {
namespace {

constexpr int species_n = static_cast<int>(species::count);
constexpr int process_n = static_cast<int>(process::count);

using sc = species_class;
using sp = species;

const species_info k_species[species_n] = {
    { "",       "none",                    sc::product      },
    // --- Volatiles ---
    { "N2",     "dinitrogen",              sc::volatile_gas },
    { "CO2",    "carbon dioxide",          sc::volatile_gas },
    { "CH4",    "methane",                 sc::volatile_gas },
    { "H2",     "dihydrogen",              sc::volatile_gas },
    { "H2O",    "water",                   sc::volatile_gas },
    { "NH3",    "ammonia",                 sc::volatile_gas },
    { "CO",     "carbon monoxide",         sc::volatile_gas },
    { "H2S",    "hydrogen sulfide",        sc::volatile_gas },
    // --- Prebiotic C/N carriers ---
    { "HCN",    "hydrogen cyanide",        sc::monomer      },
    { "HCHO",   "formaldehyde",            sc::monomer      },
    { "CH3NO",  "formamide",               sc::monomer      },
    // --- Minerals ---
    { "(Mg,Fe)2SiO4", "olivine",           sc::mineral      },
    { "Mg3Si2O5(OH)4","serpentine",        sc::mineral      },
    { "Fe3O4",  "magnetite",               sc::mineral      },
    { "Fe2O3",  "hematite",                sc::mineral      },
    { "Mg(OH)2","brucite",                 sc::mineral      },
    { "FeS",    "mackinawite",             sc::mineral      },
    { "FeS2",   "pyrite",                  sc::mineral      },
    { "Ca5(PO4)3(OH)", "apatite",          sc::mineral      },
    { "(Fe,Ni)3P",     "schreibersite",    sc::mineral      },
    // --- Ions and cofactors ---
    { "Fe2+",   "ferrous iron",            sc::ion          },
    { "Fe3+",   "ferric iron",             sc::ion          },
    { "Mg2+",   "magnesium ion",           sc::ion          },
    { "Mn",     "manganese",               sc::ion          },
    { "Mo",     "molybdenum",              sc::ion          },
    { "HPO3-",  "phosphite",               sc::ion          },
    { "PO43-",  "orthophosphate",          sc::ion          },
    { "C6H5O7", "citrate",                 sc::ion          },
    // --- Monomers ---
    { "C5H10O5","ribose",                  sc::monomer      },
    { "C5H5N5", "adenine",                 sc::monomer      },
    { "C5H5N5O","guanine",                 sc::monomer      },
    { "C4H5N3O","cytosine",                sc::monomer      },
    { "C4H4N2O2","uracil",                 sc::monomer      },
    { "C2H5NO2","glycine",                 sc::monomer      },
    { "AMP",    "nucleotide",              sc::monomer      },
    // --- Polymers ---
    { "(NMP)n", "oligonucleotide",         sc::polymer      },
    { "RNA",    "ribonucleic acid",        sc::polymer      },
    { "RNA*",   "ribozyme",                sc::polymer      },
    { "(AA)n",  "peptide",                 sc::polymer      },
    { "DNA",    "deoxyribonucleic acid",   sc::polymer      },
    // --- Membrane ---
    { "RCOOH",  "fatty acid",              sc::membrane     },
    { "[  ]",   "lipid vesicle",           sc::membrane     },
    // --- Products ---
    { "O2",     "dioxygen",                sc::product      },
    { "CH3COO-","acetate",                 sc::product      },
    { "Corg",   "organic carbon",          sc::product      },
};

const process_info k_process[process_n] = {
    { "none", "", substage::count, {}, {} },

    // --- S5a Feedstock -----------------------------------------------------
    { "hcn synthesis",
      "yield collapses in an oxidised atmosphere; adenine is literally (HCN)5",
      substage::s5a_feedstock, { sp::n2, sp::ch4 }, { sp::hcn } },
    { "formaldehyde synthesis",
      "the sugar precursor, and the formose reaction's only feedstock",
      substage::s5a_feedstock, { sp::co2, sp::h2 }, { sp::hcho } },
    { "impact reduction",
      "an iron-rich impactor reduces the whole atmosphere for ~10^5-10^6 yr",
      substage::s5a_feedstock, { sp::h2o, sp::schreibersite }, { sp::h2, sp::ch4 } },

    // --- S5b Reductant -----------------------------------------------------
    { "serpentinisation",
      "exothermic; alkaline effluent into a CO2-acidified ocean is a 3-4 pH-unit gradient",
      substage::s5b_reductant, { sp::olivine, sp::h2o },
      { sp::serpentine, sp::magnetite, sp::h2 } },
    { "wood-ljungdahl fixation",
      "the oldest carbon-fixation path, and the only one that pays for itself",
      substage::s5b_reductant, { sp::co2, sp::h2 }, { sp::acetate } },
    { "fes barrier growth",
      "Fe(Ni)S unit cells match ferredoxin and hydrogenase active sites",
      substage::s5b_reductant, { sp::fe2, sp::h2s }, { sp::mackinawite } },

    // --- S5c Phosphorylation -----------------------------------------------
    { "schreibersite corrosion",
      "meteoritic phosphide gives reactive reduced P where apatite gives none",
      substage::s5c_phosphorylation, { sp::schreibersite, sp::h2o }, { sp::phosphite } },
    { "carbonate lake release",
      "calcium drops out as carbonate, freeing phosphate to concentrate ~10^5-10^6x",
      substage::s5c_phosphorylation, { sp::apatite, sp::co2 }, { sp::orthophosphate } },
    { "nucleoside phosphorylation",
      "the step that makes a nucleotide instead of a pile of parts",
      substage::s5c_phosphorylation, { sp::ribose, sp::adenine, sp::phosphite },
      { sp::nucleotide } },

    // --- S5d Concentration -------------------------------------------------
    { "hcn oligomerisation",
      "above ~0.1 M only; below ~0.01 M hydrolysis wins and nothing accumulates",
      substage::s5d_concentration, { sp::hcn }, { sp::adenine, sp::guanine } },
    { "formose reaction",
      "formaldehyde to ribose, the sugar the backbone is built from",
      substage::s5d_concentration, { sp::hcho }, { sp::ribose } },
    { "wet-dry condensation",
      "dehydration drives it; in bulk water polymerisation is thermodynamically uphill",
      substage::s5d_concentration, { sp::nucleotide }, { sp::oligonucleotide } },
    { "thermophoretic accumulation",
      "~5000x enrichment in a pore network, and it needs no land at all",
      substage::s5d_concentration, { sp::nucleotide }, { sp::nucleotide } },

    // --- S5e Replicator ----------------------------------------------------
    { "template extension",
      "nonenzymatic copying - the RNA world's onset",
      substage::s5e_replicator, { sp::oligonucleotide, sp::nucleotide }, { sp::rna } },
    { "phosphodiester hydrolysis",
      "the competing reaction; ~10x faster per 20 C, so a hot vent erases its own product",
      substage::s5e_replicator, { sp::rna, sp::h2o }, { sp::nucleotide } },
    { "ribozyme folding",
      "catalysis and heredity in one molecule, which is why RNA came first",
      substage::s5e_replicator, { sp::rna }, { sp::ribozyme } },

    // --- S5f Compartment ---------------------------------------------------
    { "vesicle assembly",
      "without a compartment a useful ribozyme's benefit diffuses to its competitors",
      substage::s5f_compartment, { sp::fatty_acid }, { sp::vesicle } },
    { "cation flocculation",
      "Mg2+ destroys the membrane that RNA catalysis cannot work without",
      substage::s5f_compartment, { sp::vesicle, sp::mg2 }, { sp::fatty_acid } },

    // --- S5g Code ----------------------------------------------------------
    { "ribosomal translation",
      "the ribosome is itself a ribozyme - RNA built its own successor",
      substage::s5g_code, { sp::rna, sp::glycine }, { sp::peptide } },
    { "ribonucleotide reduction",
      "RNA to DNA; proofreading lifts Eigen's error threshold by orders of magnitude",
      substage::s5g_code, { sp::rna, sp::peptide }, { sp::dna } },
    { "proofreading replication",
      "the genome ceiling rises from a few thousand bases to a genuine organism",
      substage::s5g_code, { sp::dna }, { sp::dna } },

    // --- S6 ----------------------------------------------------------------
    { "photoferrotrophy",
      "one photosystem, Fe(II) as donor - banded iron with NO free oxygen at all",
      substage::s6a_anoxygenic, { sp::fe2, sp::co2 }, { sp::fe3, sp::organic_carbon } },
    { "oxygenic photosynthesis",
      "splitting water needs +1.23 V, so it takes two photosystems and an Mn4CaO5 cluster",
      substage::s6b_oxygenic, { sp::h2o, sp::co2, sp::mn }, { sp::o2, sp::organic_carbon } },
    { "nitrogen fixation",
      "N2's 941 kJ/mol triple bond yields only to Mo-dependent nitrogenase - and so does soil",
      substage::s6c_nitrogen, { sp::n2, sp::h2, sp::mo }, { sp::nh3 } },
    { "banded iron precipitation",
      "Fe(III) is insoluble by orders of magnitude, so it rains out basin-wide",
      substage::s6a_anoxygenic, { sp::fe3 }, { sp::hematite, sp::magnetite } },
    { "organic carbon burial",
      "exactly 1.00 mol O2 stranded per mol C buried - oxygenation is a sedimentology problem",
      substage::s6b_oxygenic, { sp::organic_carbon }, { sp::o2 } },
};

static_assert(sizeof(k_species) / sizeof(k_species[0]) == species_n,
              "species table out of sync with the species enum");
static_assert(sizeof(k_process) / sizeof(k_process[0]) == process_n,
              "process table out of sync with the process enum");

// RNA phosphodiester half-life in HOURS, binned every 10 C from 0 to 150.
//
// A TABLE, not exp(). PLANETOLOGY.md § Determinism & cost bans transcendentals
// in gate paths outright — they are not bit-identical across compilers or libm
// versions, and this value decides whether a world gets life. The curve runs
// ~10x per 20 C: ~4 years at 25 C, ~3 hours at 100 C.
const float k_rna_half_life[16] = {
    350000.0f, 110000.0f, 35000.0f, 11000.0f,
      3500.0f,   1100.0f,   350.0f,   110.0f,
        35.0f,     11.0f,     3.5f,     1.1f,
         0.35f,     0.11f,    0.035f,   0.011f,
};

void append_side(std::string& out, const species (&list)[3])
{
    bool first = true;
    for (int i = 0; i < 3; ++i)
    {
        if (list[i] == sp::none) continue;
        if (!first) out += " + ";
        out += info(list[i]).formula;
        first = false;
    }
    if (first) out += "-";
}

} // namespace

const species_info& info(species s)
{
    const int i = static_cast<int>(s);
    return k_species[(i >= 0 && i < species_n) ? i : 0];
}

const process_info& info(process p)
{
    const int i = static_cast<int>(p);
    return k_process[(i >= 0 && i < process_n) ? i : 0];
}

const char* substage_name(substage s)
{
    switch (s)
    {
        case substage::s5a_feedstock:       return "feedstock";
        case substage::s5b_reductant:       return "reductant";
        case substage::s5c_phosphorylation: return "phosphorylation";
        case substage::s5d_concentration:   return "concentration";
        case substage::s5e_replicator:      return "replicator";
        case substage::s5f_compartment:     return "compartment";
        case substage::s5g_code:            return "code";
        case substage::s6a_anoxygenic:      return "anoxygenic photosynthesis";
        case substage::s6b_oxygenic:        return "oxygenic photosynthesis";
        case substage::s6c_nitrogen:        return "nitrogen fixation";
        case substage::count:               break;
    }
    return "unknown";
}

const char* venue_name(venue v)
{
    switch (v)
    {
        case venue::none:              return "-";
        case venue::alkaline_vent:     return "alkaline vent";
        case venue::black_smoker:      return "black smoker";
        case venue::subaerial_pool:    return "subaerial pool";
        case venue::hydrothermal_pore: return "hydrothermal pore";
        case venue::carbonate_lake:    return "carbonate lake";
        case venue::subsurface_ocean:  return "subsurface ocean";
        case venue::open_ocean:        return "open ocean";
        case venue::atmosphere:        return "atmosphere";
        case venue::count:             break;
    }
    return "unknown";
}

const char* outcome_name(outcome o)
{
    switch (o)
    {
        case outcome::failed:   return "failed";
        case outcome::marginal: return "marginal";
        case outcome::fired:    return "fired";
        case outcome::bypassed: return "bypassed";
        case outcome::count:    break;
    }
    return "unknown";
}

float rna_half_life_hours(float temp_c)
{
    if (temp_c <= 0.0f)   return k_rna_half_life[0];
    if (temp_c >= 150.0f) return k_rna_half_life[15];

    const int   bin  = static_cast<int>(temp_c / 10.0f);
    const float frac = (temp_c - static_cast<float>(bin) * 10.0f) / 10.0f;
    const float lo   = k_rna_half_life[bin];
    const float hi   = k_rna_half_life[bin + 1];

    // Linear between bins. Crude against a true exponential, but monotone,
    // bit-identical everywhere, and the S5e threshold sits mid-range where the
    // interpolation error cannot move the verdict by more than a degree.
    return lo + (hi - lo) * frac;
}

std::string hex_bytes(const molecular_event& e)
{
    const unsigned char* raw = reinterpret_cast<const unsigned char*>(&e);
    char buf[3 * sizeof(molecular_event) + 1];
    for (size_t i = 0; i < sizeof(molecular_event); ++i)
        std::snprintf(buf + i * 3, 4, "%02x ", raw[i]);
    buf[3 * sizeof(molecular_event) - 1] = '\0';
    return std::string(buf);
}

std::string format_event(const molecular_event& e)
{
    const process_info& pi = info(static_cast<process>(e.process_id));

    char head[224];
    std::snprintf(head, sizeof head, "  %.2f Gya  %s  %s  yield %.3f  venue %s",
                  static_cast<double>(e.epoch_dmyr) / 10000.0,
                  pi.name,
                  outcome_name(static_cast<outcome>(e.outcome_id)),
                  static_cast<double>(e.yield_q) / 1000.0,
                  venue_name(static_cast<venue>(e.venue_id)));

    std::string out = hex_bytes(e);
    out += '\n';
    out += head;
    out += "\n    ";
    append_side(out, pi.reactants);
    out += " -> ";
    append_side(out, pi.products);
    return out;
}

bool tables_self_consistent()
{
    for (int i = 1; i < process_n; ++i)
    {
        const process_info& pi = k_process[i];
        if (pi.name[0] == '\0')                      return false;
        if (pi.gate == substage::count)              return false;

        bool any_reactant = false;
        for (int k = 0; k < 3; ++k)
        {
            const int r = static_cast<int>(pi.reactants[k]);
            const int p = static_cast<int>(pi.products[k]);
            if (r < 0 || r >= species_n)             return false;
            if (p < 0 || p >= species_n)             return false;
            if (pi.reactants[k] != sp::none) any_reactant = true;
        }
        if (!any_reactant)                           return false;
    }

    for (int i = 1; i < species_n; ++i)
        if (k_species[i].name[0] == '\0')            return false;

    return true;
}

} // namespace chem
