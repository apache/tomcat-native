package org.apache.naming.modules.id;

/**
 * Store int handles in a DirContext.
 *
 * This replaces the 3.3 notes mechanism ( which were stored in ContextManager ).
 * The context name is the 'namespace' for the notes ( Request, Container, etc ).
 * One subcontext will be created for each note, with the id, description, etc.
 *
 * This is also used for Coyote hooks - to create the int hook id.
 *
 * Example:
 *   id:/coyote/hooks/commit -> 1
 *   id:/coyote/hooks/pre_request -> 2 ...
 * 
 *   id:/t33/hooks/pre_request -> 1 ...
 *
 *   id:/t33/ContextManager/myNote1 -> 1
 *
 * The bound object is an Integer ( for auto-generated ids ).
 *
 * XXX Should it be a complex object ? Should we allow pre-binding of
 *  certain objects ?
 * XXX Persistence: can we bind a Reference to persistent data ? 
 */
public class IdContext {

}
