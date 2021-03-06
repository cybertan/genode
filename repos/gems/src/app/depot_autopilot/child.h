/*
 * \brief  Child representation
 * \author Norman Feske
 * \date   2018-01-23
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CHILD_H_
#define _CHILD_H_

/* Genode includes */
#include <base/log.h>
#include <util/list_model.h>
#include <base/service.h>
#include <os/reporter.h>
#include <os/buffered_xml.h>
#include <depot/archive.h>
#include <timer_session/connection.h>
#include <log_session/log_session.h>

/* local includes */
#include <list.h>

namespace Depot_deploy {

	using namespace Depot;

	class Child;
	class Event;
	class Timeout_event;
	class Log_event;
}


class Depot_deploy::Event
{
	public:

		using Meaning_string = Genode::String<12>;

		struct Invalid : Genode::Exception { };

		enum class Type { LOG, TIMEOUT };

	private:

		Meaning_string const _meaning;
		Type           const _type;

	public:

		Event(Genode::Xml_node const &node,
		      Type                    type);

		bool has_type(Type type) const { return _type == type; };

		Meaning_string const &meaning() const { return _meaning; }
};


class Depot_deploy::Log_event : public Event,
                                public List<Log_event>::Element
{
	public:

		using Line = Genode::String<Log_session::MAX_STRING_LEN + 160 + 3>;

	private:

		char           const *_base;
		Genode::size_t const  _size;
		char           const *_remaining_base;
		char           const *_remaining_end;
		bool                  _reset_retry { false };
		char           const *_reset_to;

	public:

		Log_event(Genode::Xml_node const &xml);


		/***************
		 ** Accessors **
		 ***************/

		Genode::size_t size() const { return _size; }
		char  const *  base() const { return _base; }
		char  const * &reset_to() { return _reset_to; }
		bool          &reset_retry() { return _reset_retry; }
		char  const * &remaining_base() { return _remaining_base; }
		char  const * &remaining_end() { return _remaining_end; }
};


class Depot_deploy::Timeout_event : public Event,
                                    public List<Timeout_event>::Element
{
	private:

		Child                                  &_child;
		Timer::Connection                      &_timer;
		unsigned long const                     _sec;
		Timer::One_shot_timeout<Timeout_event>  _timeout;

		void _handle_timeout(Duration);

	public:

		struct Invalid : Exception { };

		Timeout_event(Timer::Connection      &timer,
		              Child                  &child,
		              Genode::Xml_node const &event);

		/***************
		 ** Accessors **
		 ***************/

		unsigned long sec() const { return _sec; }
};


class Depot_deploy::Child : public List_model<Child>::Element
{
	public:

		typedef String<100> Name;
		typedef String<80>  Binary_name;
		typedef String<80>  Config_name;
		typedef String<32>  Depot_rom_server;
		typedef String<16>  State_name;
		typedef String<100> Launcher_name;
		typedef String<128> Conclusion;

	private:

		/*
		 * State of the condition check for generating the start node of
		 * the child. I.e., if the child is complete and configured but
		 * a used server component is missing, we need to suppress the start
		 * node until the condition is satisfied.
		 */
		enum Condition { UNCHECKED, SATISFIED, UNSATISFIED };
		enum State     { UNFINISHED, SUCCEEDED, FAILED };

		bool                    const  _skip;
		Allocator                     &_alloc;
		Reconstructible<Buffered_xml>  _start_xml;
		Constructible<Buffered_xml>    _launcher_xml       { };
		Constructible<Buffered_xml>    _pkg_xml            { };
		Condition                      _condition          { UNCHECKED };
		Name                    const  _name;
		Archive::Path                  _blueprint_pkg_path { _config_pkg_path() };
		Number_of_bytes                _pkg_ram_quota      { 0 };
		unsigned long                  _pkg_cap_quota      { 0 };
		Binary_name                    _binary_name        { };
		Config_name                    _config_name        { };
		bool                           _pkg_incomplete     { false };
		List<Timeout_event>            _timeout_events     { };
		List<Log_event>                _log_events         { };
		Timer::Connection             &_timer;
		State                          _state              { UNFINISHED };
		Signal_transmitter             _config_handler;
		bool                           _running            { false };
		Conclusion                     _conclusion         { };

		bool _defined_by_launcher() const;

		Archive::Path _config_pkg_path() const;

		Launcher_name _launcher_name() const;

		bool _configured() const;

		void _gen_routes(Xml_generator          &,
		                 Xml_node                ,
		                 Depot_rom_server const &,
		                 Depot_rom_server const &) const;

		static void _gen_provides_sub_node(Xml_generator        &xml,
		                                   Xml_node              service,
		                                   Xml_node::Type const &node_type,
		                                   Service::Name  const &service_name);

		static void _gen_copy_of_sub_node(Xml_generator        &xml,
		                                  Xml_node              from_node,
		                                  Xml_node::Type const &sub_node_type);

		void _finished(State                state,
		               Event         const &event,
		               unsigned long const  time_us);

		State_name _padded_state_name() const;

	public:

		unsigned long init_time_us { 0 };

		Child(Genode::Allocator                       &alloc,
		      Genode::Xml_node                         start_node,
		      Timer::Connection                       &timer,
		      Genode::Signal_context_capability const &config_handler);

		void log_session_write(Log_event::Line const &log_line);

		void conclusion(int &result);

		void event_occured(Event         const &event,
		                   unsigned long const  time_us);

		void apply_config(Xml_node start_node);

		void apply_blueprint(Xml_node pkg);

		void apply_launcher(Launcher_name const &name,
		                    Xml_node             launcher);

		void mark_as_incomplete(Xml_node missing);

		/**
		 * Reconsider deployment of child after installing missing archives
		 */
		void reset_incomplete();

		bool gen_query(Xml_generator &xml) const;

		/**
		 * Generate start node of init configuration
		 *
		 * \param common              session routes to be added in addition to
		 *                            the ones found in the pkg blueprint
		 * \param cached_depot_rom    name of the server that provides the depot
		 *                            content as ROM modules. If the string is
		 *                            invalid, ROM requests are routed to the
		 *                            parent.
		 * \param uncached_depot_rom  name of the depot-ROM server used to obtain
		 *                            the content of the depot user "local", which
		 *                            is assumed to be mutable
		 */
		void gen_start_node(Xml_generator          &,
		                    Xml_node                common,
		                    Depot_rom_server const &cached_depot_rom,
		                    Depot_rom_server const &uncached_depot_rom);

		/**
		 * Generate installation entry needed for the completion of the child
		 */
		void gen_installation_entry(Xml_generator &xml) const;

		template <typename FN>
		void apply_if_unsatisfied(FN const &fn) const
		{
			if (_skip) {
				return; }

			Xml_node launcher_xml = _launcher_xml.constructed()
			                      ? _launcher_xml->xml()
			                      : Xml_node("<empty/>");

			if (_condition == UNSATISFIED && _start_xml.constructed())
				fn(_start_xml->xml(), launcher_xml);
		}

		/*
		 * \return true if condition changed
		 */
		template <typename COND_FN>
		bool apply_condition(COND_FN const &fn)
		{
			if (_skip) {
				return false; }

			Condition const orig_condition = _condition;

			Xml_node launcher_xml = _launcher_xml.constructed()
			                      ? _launcher_xml->xml()
			                      : Xml_node("<empty/>");

			if (_start_xml.constructed())
				_condition = fn(_start_xml->xml(), launcher_xml)
				           ? SATISFIED : UNSATISFIED;

			return _condition != orig_condition;
		}


		/***************
		 ** Accessors **
		 ***************/

		Name name()           const { return _name; }
		bool pkg_incomplete() const { return _pkg_incomplete; }
		bool running()        const { return _running; }
		bool finished()       const { return _state != UNFINISHED; }
};

#endif /* _CHILD_H_ */
