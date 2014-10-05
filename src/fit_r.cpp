//
//  main.cpp
//  fit
//
//  Created by Alex Cooper on 4/10/2014.
//  Copyright (c) 2014 Alex Cooper. All rights reserved.
//

#include <fstream>
#include <iostream>
#include <string>
#include <set>
#include <map>
#include <vector>
#include <sstream>
#include <exception>

#include <Rcpp.h>

#include "fit_decode.hpp"
#include "fit_mesg_broadcaster.hpp"

using namespace std;
using namespace Rcpp;

// for simplicity, store all values as doubles,
// using standard C++ conversions
typedef double field_value_t;

/// A table of messages of a particular type (record, session, lap, etc.)
///
/// Fields are stored by column, mapping message number to value, which
/// is convenient for converting back to a data.frame in R.
///
/// The set of available columns is the total set of columns ever appended
/// to this table, so there might be some NAs in the final table.
class MessageTable {
private:
    typedef int message_no_t; // scoped within the table, so there will be duplicate ids
    typedef map<message_no_t, field_value_t> column_t;
    typedef vector<message_no_t> message_list_t;
    
    FIT_UINT16 num;                       // message type for this table
    vector<message_no_t> message_numbers;
    map<string, column_t> columns;
    map<string, string> column_units;

public:
    string name;                          // friendly name of table type
    message_no_t message_no;

    /// default constructor to keep STL happy
    MessageTable() { 
        message_no = 0;
    }
    
    MessageTable(FIT_UINT16 message_num, string message_name) {
        this->name = message_name;
        this->num = message_num;
        message_no = 0;
    }
    
    /// Add a message to this table
    void append_message(fit::Mesg& mesg) {
        message_numbers.push_back(++message_no);
        
        for (int i=0; i<mesg.GetNumFields(); i++) {
            fit::Field* field = mesg.GetFieldByIndex(i);
            
            bool multi_valued = field->GetNumValues() > 1;
            
            for (int j=0; j<field->GetNumValues(); j++) {
                std::string field_name;
                if (multi_valued) {
                    std::ostringstream field_name_oss;
                    field_name_oss << field->GetName() << "_" << (j+1);
                    field_name = field_name_oss.str();
                } else {
                    field_name= field->GetName();
                }
                
                if (columns.find(field_name) == columns.end()) {
                    /* need to add this field */
                    columns[field_name] = column_t();
                    column_units[field_name] = field->GetUnits();
                }
                column_t& column = columns[field_name];
                
                field_value_t value;
                switch (field->GetType())
                {
                    case FIT_BASE_TYPE_ENUM:
                        value = field->GetENUMValue(j);
                        break;
                    case FIT_BASE_TYPE_SINT8:
                        value = field->GetSINT8Value(j);
                        break;
                    case FIT_BASE_TYPE_UINT8:
                        value = field->GetUINT8Value(j);
                        break;
                    case FIT_BASE_TYPE_SINT16:
                        value = field->GetSINT16Value(j);
                        break;
                    case FIT_BASE_TYPE_UINT16:
                        value = field->GetUINT16Value(j);
                        break;
                    case FIT_BASE_TYPE_SINT32:
                        value = field->GetSINT32Value(j) ;
                        break;
                    case FIT_BASE_TYPE_UINT32:
                        value = field->GetUINT32Value(j);
                        break;
                    case FIT_BASE_TYPE_FLOAT32:
                        value = field->GetFLOAT32Value(j);
                        break;
                    case FIT_BASE_TYPE_FLOAT64:
                        value = field->GetFLOAT64Value(j);
                        break;
                    case FIT_BASE_TYPE_UINT8Z:
                        value = field->GetUINT8ZValue(j);
                        break;
                    case FIT_BASE_TYPE_UINT16Z:
                        value = field->GetUINT16ZValue(j);
                        break;
                    case FIT_BASE_TYPE_UINT32Z:
                        value = field->GetUINT32ZValue(j);
                        break;
                    default:
                        //std::cerr << "Warning: unrecognized data type "
                        //<< field->GetType() << "\n";
                        value = -1;
                        break;
                }
                column[message_no] = value;
            } // for value
        } // for field
    }
    
    DataFrame getRcppDataFrame() {
        DataFrame df = DataFrame::create();
        CharacterVector units = CharacterVector::create();
        typedef map<string, column_t>::iterator it_type;
        for (it_type i = columns.begin(); i != columns.end(); i++) {
            column_t *values = &(i->second);
            NumericVector r_col = NumericVector::create();
            
            for (message_list_t::iterator m = message_numbers.begin();
                 m != message_numbers.end(); m++) {
                if (values->find(*m) == values->end()) {
                    r_col.push_back(NA_REAL);
                } else {
                    r_col.push_back(values->at(*m));
                }
            }
            df[i->first] = r_col;
            units.push_back(column_units[i->first]);
        }
        df.attr("units") = units;
        return df;
    }
};

class Listener : public fit::MesgListener
{
public:
    typedef map<FIT_UINT16, MessageTable> table_map_t;
    table_map_t tables;

    /// Append a new 'message' to the message list. Builds a temporary data
    /// structure of field names and units, so that a data table can be constructed
    /// later with the right number of columns. (We can't assume that all messages
    /// will have all the fields, all the time.)
    void OnMesg(fit::Mesg& mesg)
    {
        if (tables.find(mesg.GetNum()) == tables.end()) {
            // new table for this message type
            tables[mesg.GetNum()] = MessageTable(mesg.GetNum(), mesg.GetName());
        }
        MessageTable& table = tables.at(mesg.GetNum());

        table.append_message(mesg);
    }
    
    List getDataFrameList() {
        List frames = List::create();
        
        for (table_map_t::iterator i = tables.begin(); i != tables.end(); i++) {
            MessageTable *mt = &(i->second);
            List dframe = mt->getRcppDataFrame();
            frames[mt->name] = dframe;
        }
        return frames;
    }
};

/// Decode a fit file.
///
/// Returns a map of tables, indexed by message type
///
// [[Rcpp::export]]
Rcpp::List decode_fit_file(SEXP f1) {
    string filename = Rcpp::as<string>(f1); 
    fstream file;
    file.open(filename.c_str(), std::ios::in | std::ios::binary);
    if (!file.is_open()) throw runtime_error("File not found ");

    fit::Decode decode;
    if (!decode.CheckIntegrity(file)) throw runtime_error("FIT file integrity failed.");
    
    fit::MesgBroadcaster mesgBroadcaster;
    Listener listener;
    mesgBroadcaster.AddListener((fit::MesgListener &)listener);
    try
    {
        mesgBroadcaster.Run(file);
    }
    catch (const fit::RuntimeException& e)
    {
        throw runtime_error(e);
    }
    return listener.getDataFrameList();
}
