--
--  \brief  Ada secondary stack
--  \author Johannes Kliemann
--  \date   2018-04-16
--
--  Copyright (C) 2018 Genode Labs GmbH
--  Copyright (C) 2018 Componolit GmbH
--
--  This file is part of the Genode OS framework, which is distributed
--  under the terms of the GNU Affero General Public License version 3.
--

package body System.Secondary_Stack is

   procedure SS_Allocate (
                          Address      : out System.Address;
                          Storage_Size : SSE.Storage_Count
                         )
   is
      T : constant Ss_Utils.Thread := Ss_Utils.C_Get_Thread;
   begin
      if T /= Ss_Utils.Invalid_Thread then
         Ss_Utils.S_Allocate (Address,
                              Storage_Size,
                              Thread_Registry,
                              T);
      else
         raise Program_Error;
      end if;
   end SS_Allocate;

   function SS_Mark return Mark_Id
   is
      M : Mark_Id;
      T : constant Ss_Utils.Thread := Ss_Utils.C_Get_Thread;
   begin
      if T /= Ss_Utils.Invalid_Thread then
         Ss_Utils.S_Mark (M.Sstk,
                          SSE.Storage_Count (M.Sptr),
                          Thread_Registry,
                          T);
      else
         raise Program_Error;
      end if;
      return M;
   end SS_Mark;

   procedure SS_Release (
                         M : Mark_Id
                        )
   is
      T : constant Ss_Utils.Thread := Ss_Utils.C_Get_Thread;
   begin
      if T /= Ss_Utils.Invalid_Thread then
         Ss_Utils.S_Release (M.Sstk,
                             SSE.Storage_Count (M.Sptr),
                             Thread_Registry,
                             T);
      else
         raise Program_Error;
      end if;
   end SS_Release;

end System.Secondary_Stack;
