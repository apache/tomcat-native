/*
 * Copyright 1999,2004 The Apache Software Foundation.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *      http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


package org.apache.tomcat.util.loader;

public interface ModuleListener {
    
    /** Called when a module group is created. This is only called when a new group
     * is added to a running engine - we may cache a the list of groups and reuse 
     * it on restarts. 
     * 
     * @param manager
     */
    public void repositoryAdd( Repository manager );
    
    /** Notification that a module has been added. You can get the group
     * with getGroup().
     * 
     * Adding a module doesn't imply that the module is started or the class loader
     * created - this happens only on start() ( TODO: or when a class is accessed ? ).
     * 
     * This callback is only called for new modules, deployed while the engine is 
     * running, or in some cases when the module engine is reseting the cache. For
     * old modules - you need to get a list from the ModuleGroup.
     * 
     * 
     * @param module
     */
    public void moduleAdd( Module module );
    
    /** Notification that a module has been removed.
     * 
     * @param module
     */
    public void moduleRemove( Module module );
    
    /** Module reload - whenever reload happens, a reload notification will be generated.
     * Sometimes a remove/add will do the same.
     * 
     * @param module
     */
    public void moduleReload( Module module );
    
    /** Called when a module is started. 
     * 
     * This is called after the class loader is created, to allow listeners to use classes
     * from the module to initialize.
     * 
     * I think it would be good to have the module 'started' on first class loaded
     * and 'stopped' explicitely.
     * 
     * @param module
     */
    public void moduleStart(Module module);
    
    /** 
     *  Called when a module is stopped. Stoping a module will stop the class
     * loader and remove all references to it. When a module is stopped, all attempts
     * to load classes will result in exceptions. 
     * 
     * The callback is called before the class loader is stopped - this allows listeners
     * to use classes from the module to deinitialize.
     * 
     * @param module
     */
    public void moduleStop(Module module);
    
    /** Pass a reference to the loader. 
     * 
     * This is the only supported way to get it - no static methods are 
     * provided. From loader you can control all repositories and modules.
     * 
     * Note that ModuleClassLoader does not provide a way to retrieve the Module -
     * you need to have a reference to the Loader to get the Module for a 
     * ClassLoader. 
     * @param main
     */
    public void setLoader(Loader main);

    /** Start the listener.
     * TODO: this is only used by Loader to pass control to the listener - 
     * instead of introspection for main()
     */
    public void start();

}