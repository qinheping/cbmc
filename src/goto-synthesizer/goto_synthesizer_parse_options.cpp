/*******************************************************************\

Module: Command Line Parsing

Author: Qinheping Hu, qinhh@amazon.com

\*******************************************************************/

/// \file
/// Command Line Parsing
#include "goto_synthesizer_parse_options.h"
#include "goto_synthesizer_enumerator.h"
#include "goto_synthesizer_verifier.h"

#include <fstream>
#include <iostream>
#include <memory>

#include <util/config.h>
#include <util/version.h>
#include <util/exit_codes.h>
#include <util/c_types.h>
#include <util/range.h>
#include <util/string_constant.h>
#include <util/unicode.h>
#include <util/pointer_predicates.h>

#include <ansi-c/c_expr.h>

#include <langapi/language_util.h>

#ifdef _MSC_VER
#endif


#include <goto-programs/initialize_goto_model.h>
#include <goto-programs/link_to_library.h>
#include <goto-programs/loop_ids.h>
#include <goto-programs/read_goto_binary.h>
#include <goto-programs/show_goto_functions.h>
#include <goto-programs/show_properties.h>
#include <goto-programs/show_symbol_table.h>
#include <goto-programs/goto_function.h>
#include <goto-programs/goto_program.h>

#include <langapi/mode.h>

#include <ansi-c/ansi_c_language.h>
#include <cpp/cpp_language.h>

#include <goto-instrument/loop_utils.h>
#include <goto-instrument/contracts/contracts.h>

#include <cbmc/cbmc_parse_options.h>

void goto_synthesizer_parse_optionst::register_languages()
{
  register_language(new_ansi_c_language);
  register_language(new_cpp_language);
}

bool goto_synthesizer_parse_optionst::call_back(const exprt &expr)
{
  simple_verifiert v(*this, this->ui_message_handler);
  if(v.verify(expr))
  {
    std::cout<< "Candidate: " << from_expr(expr) <<"\n";
    log.result() << "result : " << log.green << "PASS" << messaget::eom << log.reset;

    return true;
  }
  else
  {
    //log.result() << "result : " << log.red << "FAIL" << messaget::eom << log.reset;

    return false;
  }
}

void goto_synthesizer_parse_optionst::extract_exprt(const exprt &expr)
{
  if(expr.operands().size() != 0)
  {
    for(const auto &operand : expr.operands())
    {
      extract_exprt(operand);
    }
  }
  else
  {
    for(const auto &symbol : terminal_symbols)
    {
      if(expr == symbol)
        return;
    }
    // log.status() << from_expr(expr) << messaget::eom;
    // log.status() << expr.type().pretty() << messaget::eom;
    if(expr.type() == bitvector_typet(ID_signedbv,32) || expr.type() == bitvector_typet(ID_unsignedbv,32))
      terminal_symbols.push_back(expr);
  }
}

void goto_synthesizer_parse_optionst::synthesize_loop_contracts(
  const irep_idt &function_name,
  goto_functionst::goto_functiont &goto_function,
  const goto_programt::targett loop_head,
  const loopt &loop
)
{
  PRECONDITION(!loop.empty());

  target_function_name = function_name;

  // find the last back edge
  goto_programt::targett loop_end = loop_head;
  for(const auto &t : loop)
  {
    if(
      t->is_goto() && t->get_target() == loop_head &&
      t->location_number > loop_end->location_number)
      loop_end = t;
  }
  
  exprt current_candidate = true_exprt();
  exprt one = from_integer(1, size_type());
  exprt zero = from_integer(0, size_type());
  //terminal_symbols.push_back(one);

  while (true)
  {
  
    simple_verifiert v(*this, this->ui_message_handler);
    if(v.verify(current_candidate))
    {
      log.result() << "result : " << log.green << "PASS" << messaget::eom << log.reset;
      return;
    }
    else
    {
      log.result() << "result : " << log.red << "FAIL" << messaget::eom << log.reset;
    }

    if(terminal_symbols.empty())
    {
      log.result() << "start to construct \n";
      for(exprt lhs: v.return_cex.live_lhs)
      {

        std::cout << "live1 : " << from_expr(lhs) << "\n";
        if(lhs.type().id() == ID_unsignedbv)
        { 
          terminal_symbols.push_back(lhs);
          terminal_symbols.push_back(unary_exprt(ID_loop_entry, lhs, size_type()));
          //idx_type = lhs.type();
        }
        if((lhs.type().id() == ID_pointer))
        {
          // terminal_symbols.push_back(unary_exprt(ID_object_size, lhs, size_type()));
          terminal_symbols.push_back(unary_exprt(ID_pointer_offset, lhs, size_type()));
          terminal_symbols.push_back(unary_exprt(ID_pointer_offset, unary_exprt(ID_loop_entry, lhs, lhs.type()), size_type()));
          if(lhs.type().subtype().id() == ID_unsignedbv){
            //TODO terminal_symbols.push_back(dereference_exprt(lhs));
          }
        }
      }
    }


    if(v.return_cex.cex_type == cex_null_pointer)
    {
      exprt original_checked_pointer = v.checked_pointer;
      current_candidate = and_exprt(current_candidate, same_object(original_checked_pointer , unary_exprt(ID_loop_entry, original_checked_pointer)));
    }
    else if(deductive && v.return_cex.cex_type == cex_oob)
    {
      exprt offset = v.offset;
      exprt deref_object = v.dereferenced_object;

      // TODO: modify this!
      exprt four = from_integer(0, size_type());
    
      //current_candidate = and_exprt(current_candidate, and_exprt(less_than_or_equal_exprt(zero, offset), less_than_or_equal_exprt(offset, deref_object)));
      current_candidate = and_exprt(current_candidate, offset);
    } 
    else
    {
      simple_enumeratort enumerator(*this, current_candidate, v.return_cex);  
      if(enumerator.enumerate())
        break;
    } 
  }
}

void goto_synthesizer_parse_optionst::synthesize_loop_contracts(
  const irep_idt &function_name,
  goto_functionst::goto_functiont &goto_function)
{
  natural_loops_mutablet natural_loops(goto_function.body);

  // Iterate over the (natural) loops in the function,
  // and syntehsize loop invaraints.
  for(const auto &loop : natural_loops.loop_map)
  {
    synthesize_loop_contracts(
      function_name,
      goto_function,
      loop.first,
      loop.second);
  }
}

void goto_synthesizer_parse_optionst::synthesize_loop_contracts(goto_functionst &goto_functions)
{
  for(auto &goto_function : goto_functions.function_map){
    synthesize_loop_contracts(goto_function.first, goto_function.second);
  }
}

/// invoke main modules
int goto_synthesizer_parse_optionst::doit()
{
  if(cmdline.isset("version"))
  {
    std::cout << CBMC_VERSION << '\n';
    return CPROVER_EXIT_SUCCESS;
  }

  if(cmdline.args.size()!=1 && cmdline.args.size()!=2 && cmdline.args.size()!=3)
  {
    help();
    return CPROVER_EXIT_USAGE_ERROR;
  }
  
  messaget::eval_verbosity(
    cmdline.get_value("verbosity"), messaget::M_STATISTICS, ui_message_handler);

  {
    register_languages();

    get_goto_program();

    {
      const bool validate_only = cmdline.isset("validate-goto-binary");

      if(validate_only || cmdline.isset("validate-goto-model"))
      {
        goto_model.validate(validation_modet::EXCEPTION);

        if(validate_only)
        {
          return CPROVER_EXIT_SUCCESS;
        }
      }
      log.status() << "Model is valid" << messaget::eom;

    }

    symbol_table = goto_model.symbol_table;

    if(cmdline.isset("deductive"))
      deductive = true;
    
    if(cmdline.isset("hybrid"))
      hybrid = true;

    synthesize_loop_contracts(goto_model.goto_functions);
    return CPROVER_EXIT_SUCCESS;
  }

  help();
  return CPROVER_EXIT_USAGE_ERROR;
}



void goto_synthesizer_parse_optionst::get_goto_program()
{
  log.status() << "Reading GOTO program from '" << cmdline.args[0] << "'"
               << messaget::eom;

  config.set(cmdline);

  auto result = read_goto_binary(cmdline.args[0], ui_message_handler);

  if(!result.has_value())
    throw 0;

  goto_model = std::move(result.value());

  config.set_from_symbol_table(goto_model.symbol_table);
}

/// display command line help
void goto_synthesizer_parse_optionst::help()
{
  // clang-format off
  std::cout << '\n' << banner_string("InvSynthesizer", CBMC_VERSION) << '\n'
            << align_center_with_border("Copyright (C) 2008-2013") << '\n'
            << align_center_with_border("Qinheping Hu") << '\n'
            << align_center_with_border("qinhh@amazon.com") << '\n'
            <<
    "\n"
    "Usage:                       Purpose:\n"
    "\n"
    " goto-synthesizer [-?] [-h] [--help]  show help\n"
    " goto-synthesizer in out              perform synthesizing\n"
    "\n"
    "Main options:\n"
    "\n"
    "Other options:\n"
    " --no-system-headers          with --dump-c/--dump-cpp: generate C source expanding libc includes\n" // NOLINT(*)
    " --use-all-headers            with --dump-c/--dump-cpp: generate C source with all includes\n" // NOLINT(*)
    " --harness                    with --dump-c/--dump-cpp: include input generator in output\n" // NOLINT(*)
    " --version                    show version and exit\n"
    HELP_FLUSH
    " --xml-ui                     use XML-formatted output\n"
    " --json-ui                    use JSON-formatted output\n"
    HELP_TIMESTAMP
    "\n";
  // clang-format on
}
