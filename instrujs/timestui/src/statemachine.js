/* $Id: statemachine.js, v1.0 2019/11/30 VaderDarth Exp $
 * OpenCPN dashboard_tactics plug-in
 * Licensed under MIT - see distribution.
 */
/*
  Visualize with GraphViz, see iface.getgraphvizout(), feed a web app
  Note: with javascript-state-machine, avoid keyword 'init' in state/transition!
*/
var dbglevel = window.instrustat.debuglevel

import StateMachine from 'javascript-state-machine'
import getLocInfo from '../../src/location'
import {initLoad} from './init'
import {getidAskClient, getidClientAnswer} from './getid'
import {getConf, getPathDefaultsIfNew, clearConf, prepareConfHalt} from '../../src/conf'
import {getalldbAskClient, getalldbClientAnswer, getpathAskClient, gotAckCheckSchema, getschemaAcknowledged} from './path'
import {dataQuery, setIdbClientStateHasResult, setIdbClientStateGotError} from './idbclient'
import {setMenuAllPaths, setMenuRunTime, setMenuBackToLoading} from '../../src/menu'
import {onWaitdataFinalCheck, showData, clearData, noData, prepareDataHalt} from './data'
import {swapDisplay, rollDisplayToSelection} from './disp'
import {getNewLuminosity} from './css'

function dbgPrintFromTo( stateOrTransStr, lifecycle ) {
    console.log( stateOrTransStr )
    console.log('- transition: ', lifecycle.transition)
    console.log('- from      : ', lifecycle.from)
    console.log('- to        : ', lifecycle.to)
}

export function createStateMachine() {
    return new StateMachine({
        init: 'window',
        data: {
            // Static
            uid        : '',
            conf       : null,
            perspath   : false,
            // Environmental
            luminosity : 'day',
            locInfo    : getLocInfo(),
            // Functional
            databusy   : false,
            chart      : [],
            // Signal K Paths
            path       : '',
            allpaths   : [],
            menu       : null,
        },
        transitions: [
            { name: 'init',      from: 'window',   to: 'loading' },
            { name: 'loaded',    from: 'loading',  to: 'initga' },
            { name: 'initok',    from: 'initga',   to: 'getid' },
            { name: 'setid',     from: 'getid',    to: 'hasid' },
            { name: 'nocfg',     from: 'hasid',    to: 'getalldb' },
            { name: 'hascfg',    from: 'hasid',    to: 'getschema' },
            { name: 'setalldb',  from: 'getalldb', to: 'showmenu' },
            { name: 'rescan',    from: 'showmenu', to: 'getalldb' },
            { name: 'selected',  from: 'showmenu', to: 'getschema' },
            { name: 'ackschema', from: 'getschema',to: 'getdata' },
            { name: 'getnew',    from: 'showdata', to: 'getdata' },
            { name: 'newdata',   from: 'getdata',  to: 'showdata' },
            { name: 'errdata',   from: 'getdata',  to: 'nodata' },
            { name: 'retryget',  from: 'nodata',   to: 'getdata' },
            { name: 'chgconf',   from: 'getschema',to: 'getalldb' },
            { name: 'chgconf',   from: 'showdata', to: 'getalldb' },
            { name: 'chgconf',   from: 'nodata',   to: 'getalldb' },
            { name: 'luminsty',  from: 'getid',    to: 'getid' },
            { name: 'luminsty',  from: 'hasid',    to: 'hasid' },
            { name: 'luminsty',  from: 'getschema',to: 'getschema' },
            { name: 'luminsty',  from: 'getalldb', to: 'getalldb' },
            { name: 'luminsty',  from: 'getdata',  to: 'getdata' },
            { name: 'luminsty',  from: 'nodata',   to: 'nodata' },
            { name: 'luminsty',  from: 'showmenu', to: 'showmenu' },
            { name: 'luminsty',  from: 'showdata', to: 'showdata' },
            { name: 'closing',   from: 'nodata',   to: 'halt' },
            { name: 'closing',   from: 'showdata', to: 'halt' }
        ],
        methods: {
            onWindow:   function() {
                if ( dbglevel > 0 ) console.log('onWindow() - state')
            },
            onLoading:  function() {
                if ( dbglevel > 0 ) console.log('onLoading() - state')
                if ( dbglevel > 1 ) console.log('uid: ', this.uid )
                if ( dbglevel > 1 ) console.log('locInfo: ', this.locInfo )
                if ( dbglevel > 1 ) console.log('chart[', this.chart.length, ']')
                if ( dbglevel > 1 ) console.log('conf: ', this.conf)
            },
            onInitga:   function() {
                if ( dbglevel > 0 ) console.log('onInitga() - state')
                initLoad( this )
            },
            onGetid:    function() {
                if ( dbglevel > 0 ) console.log('onGetid() - state')
                getidAskClient()
            },
            onBeforeSetid:    function() {
                if ( dbglevel > 0 ) console.log('onSetid() - before transition')
                getidClientAnswer( this )
                if ( dbglevel > 1 ) console.log('uid : ', this.uid )
                getConf( this )
                if ( dbglevel > 1 ) console.log('conf: ', this.conf )
                rollDisplayToSelection( this )
            },
            onHasid:    function() {
                if ( dbglevel > 0 ) console.log('onHasid() - state')
            },
            onBeforeNocfg: function( lifecycle ) {
                if ( dbglevel > 0 ) console.log('onNocfg() - before transition')
                if ( dbglevel > 2)
                    dbgPrintFromTo( 'onBeforeNocfg', lifecycle )
                this.perspath = false
            },
            onBeforeRescan: function() {
                if ( dbglevel > 0 ) console.log('onRescan() - before transition')
                setMenuBackToLoading( this )
                clearData( this )
                clearConf( this )
            },
            onBeforeChgconf: function() {
                if ( dbglevel > 0 ) console.log('onChgconf() - before transition')
                setMenuBackToLoading( this )
                clearData( this )
                clearConf( this )
            },
            onGetalldb:   function() {
                if ( dbglevel > 0 ) console.log('onGetalldb() - state')
                getalldbAskClient()
            },
            onShowmenu:  function() {
                if ( dbglevel > 0 ) console.log('onShowmenu() - state')
                getalldbClientAnswer( this )
                if ( dbglevel > 1 ) console.log('allpaths: ', this.allpaths )
                setMenuAllPaths( this )
            },
            onBeforeHascfg: function( lifecycle ) {
                dbgPrintFromTo( 'onHascfg() - before transition', lifecycle )
                this.perspath = true
                setMenuRunTime( this )
            },
            onBeforeSelected: function() {
                if ( dbglevel > 0 ) console.log('onSelected() - before transition')
                setMenuRunTime( this )
                this.path = window.iface.getselected()
                getPathDefaultsIfNew ( this )
            },
            onGetschema:  function( lifecycle ) {
                dbgPrintFromTo( 'onGetschema() - state', lifecycle )
                getpathAskClient( this )
            },
            onBeforeAckschema:   function() {
                if ( dbglevel > 0 )
                    console.log('onAckschema() - before transition (BT)')
                clearData( this )
                if ( dbglevel > 1 )
                    console.log('onAckschema() BT - clearData() done')
                gotAckCheckSchema( this )
                if ( dbglevel > 1 )
                    console.log('onAckschema() BT - gotAckCheckSchema() done')
                getschemaAcknowledged( this )
                if ( dbglevel > 1 )
                    console.log('onAckschema() BT - getschemaAcknowledged() done')
                onWaitdataFinalCheck( this )
                if ( dbglevel > 1 )
                    console.log('onAckschema() BT - onWaitdataFinalCheck() done')
                dataQuery()
                if ( dbglevel > 1 )
                    console.log('onAckschema() BT - dataQuery() done')
            },
            onAfterAckschema:   function() {
                if ( dbglevel > 0 )
                    console.log('onAckschema() - after transition')
            },
            onBeforeGetnew:   function() {
                if ( dbglevel > 0 )
                    console.log('onGetnew() - before transition (BT) - dataQuery()')
                clearData( this )
                if ( dbglevel > 1 )
                    console.log('onGetnew() BT - clearData() done')
                dataQuery()
                if ( dbglevel > 1 )
                    console.log('onGetnew() BT - dataQuery() done')
            },
            onAfterGetnew:   function() {
                if ( dbglevel > 0 )
                    console.log('onGetnew() - after transition')
            },
            onGetdata: function() {
                if ( dbglevel > 0 )
                    console.log('onGetdata() - state')
            },
            onBeforeNewdata: function() {
                if ( dbglevel > 0 )
                    console.log('onNewdata() - before transition (BT)')
                setIdbClientStateHasResult()
                if ( dbglevel > 1 )
                    console.log('onShowData() BT - databusy = true')
                this.databusy = true
                showData( this )
                if ( dbglevel > 1 )
                    console.log('onShowData() BT - showData() done')
            },
            onShowdata: function() {
                if ( dbglevel > 0 )
                    console.log('onShowData() - state (ST)')
//                while ( this.databusy ) {

// var testval = getAddedDataCount()
// console.log('testval: ', testval)
// var test = new Promise(function(resolve, reject) {
//    setTimeout(() => {
//       console.log('in Promise, testval: ', testval)
//       if ( testval === 0 ) {
//          console.log('testval === 0')
//           resolve(0)
//       }
//       else {
//          reject(testval)
//       }
//    }, 3 * 1000)
// })
// // Success or not success and reason
// test.then(
//    success => console.log('success or failure: success! ', success),
//    failure => console.log('success or failure: failure! reason: ', failure)
// )
// // deal only with fullfilled case
// test.then(
//    value => console.log('check success only: value: ', value)
// )
// // deal only with rejected case
// test.then(
//    undefined,
//    failure => console.log('check for failure only: reason ', failure)
// )
// test.catch(
//     reason => console.log('catch:', reason)
// )


                    // var addedData = new getAddedDataCountWithDelay()
                    // addedData.then(
                    //     isempty  => {
                    //         if ( dbglevel > 1 )
                    //             console.log(
                    //                 'onShowData() - state (ST): addedData isempty: ',
                    //                 isempty )
                    //         this.databusy = false
                    //     },
                    //     notempty => {
                    //         if ( dbglevel > 1 )
                    //             console.log(
                    //                 'onShowData() - state (ST): addedData notempty: ',
                    //                 notempty )
                    //     }
                    // )
//                }
                if ( dbglevel > 1 )
                    console.log('onShowData() ST - done.')
            },
            onBeforeErrdata: function() {
                if ( dbglevel > 0 )
                    console.log('onNodata() - before transition (errdata)')
                setIdbClientStateGotError()
            },
            onNodata: function() {
                if ( dbglevel > 0 )
                    console.log('onNoData() - state')
                noData( this )
            },
            onBeforeRetryget: function( lifecycle ) {
                if ( dbglevel > 0 ) console.log('onRetryget() - before transition (BT)')
                if ( dbglevel > 2)
                    dbgPrintFromTo( 'onBeforeRetryget (BT)', lifecycle )
                setMenuBackToLoading( this )
                dataQuery()
                if ( dbglevel > 1 )
                    console.log('onBeforeRetryget BT - dataQuery() done')
            },
            onBeforeLuminsty: function() {
                if ( dbglevel > 0 )
                    console.log('onLuminsty() - before transition')
                getNewLuminosity( this )
                if ( dbglevel > 1 )
                    console.log('luminosity: ', this.luminosity )
            },
            onBeforeClosing: function() {
                if ( dbglevel > 0 )
                    console.log('onClosing() - before transition')
                prepareDataHalt( this )
                prepareConfHalt( this )
            }
        }
    })
}
