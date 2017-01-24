/*
 * mosquitto_transport
 */

/*!
 * \file
 * \brief Helpers for spliting topic name to parts.
 * \since
 * v.0.3.0
 */

#pragma once

#include <mosquitto_transport/impl/fragments_extractor.hpp>

#include <boost/container/flat_set.hpp>
#include <boost/container/map.hpp>

#include <iterator>

namespace mosquitto_transport {

namespace impl {

namespace bcnt = boost::container;

//
// subscriptions_map_t
//
/*!
 * \brief Subscriptions container.
 *
 * \tparam POSTMAN type of subsciber to be stored with topic filter.
 */
template< typename POSTMAN >
class subscriptions_map_t
	{
		subscriptions_map_t( const subscriptions_map_t & ) = delete;
		subscriptions_map_t( subscriptions_map_t && ) = delete;

	public :
		using postman_type = POSTMAN;

		subscriptions_map_t()
			{}

		void
		insert(
			const std::string & topic_filter,
			POSTMAN postman );

		std::vector< POSTMAN >
		match( const std::string & topic_name ) const;

		void
		erase(
			const std::string & topic_filter,
			POSTMAN postman );

	private :
		struct tree_item_t
			{
				//! Postmans for this node.
				bcnt::flat_set< POSTMAN > m_postmans;

				//! Children with non-wildcard names.
				bcnt::map< std::string, tree_item_t > m_children;

				//! Subtree for child node with '+'.
				std::unique_ptr< tree_item_t > m_plus_subtree;

				//! Postmans for child node with '#'.
				bcnt::flat_set< POSTMAN > m_grid_postmans;

				enum remove_action_t
				{
					keep_node,
					remove_node
				};
			};

		//! The root of subscription tree.
		/*!
		 * Doesn't have a name. Cannot be deleted.
		 */
		tree_item_t m_root;

		static void
		insert_subscription(
			tree_item_t * root,
			const fragments_extractor_t fragments,
			POSTMAN postman );

		static void
		collect_postmans(
				const tree_item_t * root,
				const fragments_extractor_t fragments,
				std::vector< POSTMAN > & result );

		static typename tree_item_t::remove_action_t
		remove_subscription(
			tree_item_t * root,
			const fragments_extractor_t fragments,
			POSTMAN postman ) noexcept;

		static bool
		is_one_level_wildcard( const std::string & topic_subname )
			{
				return "+" == topic_subname;
			}

		static bool
		is_multi_level_wildcard( const std::string & topic_subname )
			{
				return "#" == topic_subname;
			}
	};

template< typename POSTMAN >
void
subscriptions_map_t< POSTMAN >::insert(
	const std::string & topic_filter,
	POSTMAN postman )
	{
		const auto parsed_topic = split_topic_name( topic_filter );

		// Helper object to guarantee exception safety.
		// Do erase for topic_filter+postman as a roolback action
		// in case of any exception.
		struct insert_rollback {
			subscriptions_map_t & m_map;
			const std::string & m_topic_filter;
			POSTMAN m_postman;
			bool m_commited = false;

			insert_rollback(
				subscriptions_map_t & map,
				const std::string & topic_filter,
				POSTMAN postman )
				:	m_map{ map }
				,	m_topic_filter{ topic_filter }
				,	m_postman{ postman }
				{}
			~insert_rollback()
				{
					if(!m_commited)
						m_map.erase( m_topic_filter, m_postman );
				}
			void commit() { m_commited = true; }
		}
		rollback{ *this, topic_filter, postman };

		insert_subscription(
				&m_root,
				fragments_extractor_t{ parsed_topic }, postman );

		rollback.commit();
	}

template< typename POSTMAN >
std::vector< POSTMAN >
subscriptions_map_t< POSTMAN >::match( const std::string & topic_name ) const
	{
		std::vector< POSTMAN > result;
		collect_postmans(
				&m_root,
				fragments_extractor_t{ split_topic_name( topic_name ) },
				result );
		return result;
	}

template< typename POSTMAN >
void
subscriptions_map_t< POSTMAN >::erase(
	const std::string & topic_filter,
	POSTMAN postman )
	{
		remove_subscription(
				&m_root,
				fragments_extractor_t{ split_topic_name( topic_filter ) },
				postman );
	}

template< typename POSTMAN >
void
subscriptions_map_t< POSTMAN >::insert_subscription(
	tree_item_t * root,
	const fragments_extractor_t fragments,
	POSTMAN postman )
	{
		if( !fragments )
			{
				// This is the last fragment. Postman must be added to
				// the current root.
				root->m_postmans.insert( postman );
			}
		else
			{
				if( is_one_level_wildcard( *fragments ) )
					{
						if( !root->m_plus_subtree )
							root->m_plus_subtree = std::make_unique< tree_item_t >();

						insert_subscription(
								root->m_plus_subtree.get(),
								fragments.next(),
								postman );
					}
				else if( is_multi_level_wildcard( *fragments ) )
					{
						root->m_grid_postmans.insert( postman );
					}
				else
					{
						// Note: if there was no item with the current name
						// it will be created automatically.
						auto & item = root->m_children[ *fragments ];
						insert_subscription(
								&item,
								fragments.next(),
								postman );
					}
			}
	}

template< typename POSTMAN >
void
subscriptions_map_t< POSTMAN >::collect_postmans(
	const tree_item_t * root,
	const fragments_extractor_t fragments,
	std::vector< POSTMAN > & result )
	{
		if( !fragments )
			// All postmans from the current node must go to result.
			copy( begin(root->m_postmans), end(root->m_postmans),
					back_inserter(result) );
		else
			{
				// There is another part of name and we should search this
				// part between children nodes.

				auto itchild = root->m_children.find( *fragments );
				if( itchild != root->m_children.end() )
					{
						collect_postmans(
								&(itchild->second),
								fragments.next(),
								result );
					}

				if( root->m_plus_subtree )
					collect_postmans(
							root->m_plus_subtree.get(),
							fragments.next(),
							result );
			}

		// This behaviour is necessary for handling cases like:
		// topic_filter is 'foo/#', topic_name is 'foo'.
		// In this case '#' must match parent segment (e.g. 'foo').
		copy( begin(root->m_grid_postmans), end(root->m_grid_postmans),
				back_inserter(result) );
	}

template< typename POSTMAN >
typename subscriptions_map_t< POSTMAN >::tree_item_t::remove_action_t
subscriptions_map_t< POSTMAN >::remove_subscription(
	tree_item_t * root,
	const fragments_extractor_t fragments,
	POSTMAN postman ) noexcept
	{
		if( !fragments )
			root->m_postmans.erase( postman );
		else if( is_one_level_wildcard( *fragments ) )
			{
				if( root->m_plus_subtree )
					{
						auto r = remove_subscription(
								root->m_plus_subtree.get(),
								fragments.next(),
								postman );
						if( tree_item_t::remove_node == r )
							// This subtree is no more needed.
							root->m_plus_subtree.reset();
					}
			}
		else if( is_multi_level_wildcard( *fragments ) )
			{
				root->m_grid_postmans.erase( postman );
			}
		else
			{
				auto itchild = root->m_children.find( *fragments );
				if( itchild != root->m_children.end() )
					{
						auto r = remove_subscription(
								&(itchild->second),
								fragments.next(),
								postman );
						if( tree_item_t::remove_node == r )
							root->m_children.erase( itchild );
					}
			}

		if( root->m_postmans.empty() && root->m_children.empty() &&
				!(root->m_plus_subtree) && root->m_grid_postmans.empty() )
			return tree_item_t::remove_node;
		else
			return tree_item_t::keep_node;
	}

} /* namespace impl */

} /* namespace mosquitto_transport */

